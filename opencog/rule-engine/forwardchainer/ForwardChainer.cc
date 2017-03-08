/*
 * ForwardChainer.cc
 *
 * Copyright (C) 2014,2015 OpenCog Foundation
 *
 * Author: Misgana Bayetta <misgana.bayetta@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/algorithm/unique_copy.hpp>

#include <opencog/atoms/execution/Instantiator.h>
#include <opencog/atoms/pattern/BindLink.h>
#include <opencog/atoms/pattern/PatternLink.h>
#include <opencog/atomutils/FindUtils.h>
#include <opencog/atomutils/Substitutor.h>
#include <opencog/query/BindLinkAPI.h>
#include <opencog/query/DefaultImplicator.h>
#include <opencog/rule-engine/Rule.h>

#include "ForwardChainer.h"
#include "FCLogger.h"

using namespace opencog;

ForwardChainer::ForwardChainer(AtomSpace& as, const Handle& rbs,
                               const Handle& hsource,
                               const HandleSeq& focus_set /* = HandleSeq()*/,
                               source_selection_mode sm /*= source_selection_mode::UNIFORM */) :
    _as(as), _rec(as), _rbs(rbs), _configReader(as, rbs), _fcstat(as)
{
    _ts_mode = sm;
    init(hsource, focus_set);
}

ForwardChainer::~ForwardChainer()
{
}

void ForwardChainer::init(const Handle& hsource, const HandleSeq& focus_set)
{
    validate(hsource, focus_set);

    _search_in_af = _configReader.get_attention_allocation();
    _search_focus_set = not focus_set.empty();

    // Set potential source.
    HandleSeq init_sources;

    // Accept set of initial sources wrapped in a SET_LINK.
    if (hsource->getType() == SET_LINK) {
        init_sources = hsource->getOutgoingSet();
    } else {
        init_sources.push_back(hsource);
    }
    update_potential_sources(init_sources);

    // Add focus set atoms and sources to focus_set atomspace
    if (_search_focus_set) {
        _focus_set = focus_set;

        for (const Handle& h : _focus_set)
            _focus_set_as.add_atom(h);

        for (const Handle& h : _potential_sources)
            _focus_set_as.add_atom(h);
    }

    // Set rules.
    _rules = _configReader.get_rules();

    // Reset the iteration count and max count
    _iteration = 0;
    _max_iteration = _configReader.get_maximum_iterations();
}

void ForwardChainer::do_chain()
{
    // Relex2Logic uses this. TODO make a separate class to handle
    // this robustly.
    if(_potential_sources.empty())
    {
        apply_all_rules();
        return;
    }

    while (not termination())
    {
        do_step();
    }

    fc_logger().debug("Finished forwarch chaining");
}

void ForwardChainer::do_step()
{
	fc_logger().debug("Iteration %d", _iteration);
	_iteration++;

	// Select source
	_cur_source = select_source();
	LAZY_FC_LOG_DEBUG << "Source:" << std::endl << _cur_source->toString();

	// Select rule
	Rule rule = select_rule(_cur_source);
	if (not rule.is_valid()) {
		fc_logger().debug("No selected rule, abort step");
		return;
	}

	// Apply rule on _cur_source
	UnorderedHandleSet products = apply_rule(rule);

	// Store results
	update_potential_sources(products);
	_fcstat.add_inference_record(_iteration - 1, // _iteration has
	                             // already been
	                             // incremented
	                             _cur_source, rule, products);
}

bool ForwardChainer::termination()
{
    return _max_iteration <= _iteration;
}

/**
 * Applies all rules in the rule base.
 *
 * @param search_focus_set flag for searching focus set.
 */
void ForwardChainer::apply_all_rules()
{
    for (const Rule& rule : _rules) {
        fc_logger().debug("Apply rule %s", rule.get_name().c_str());
        UnorderedHandleSet uhs = apply_rule(rule);

        // Update
        _fcstat.add_inference_record(_iteration,
                                     _as.add_node(CONCEPT_NODE, "dummy-source"),
                                     rule, uhs);
        update_potential_sources(uhs);
    }
}

UnorderedHandleSet ForwardChainer::get_chaining_result()
{
    return _fcstat.get_all_products();
}

Handle ForwardChainer::select_source()
{
	size_t selsrc_size = _selected_sources.size();
	// If all sources have been selected then insert the sources'
	// children in the set of potential sources
	if (_unselected_sources.empty()) {
		fc_logger().debug() << "All " << selsrc_size
		                    << " sources have already been selected";

		// Hack to help to exhaust sources with
		// multiple matching rules. This would be
		// better used with a memory of which
		// source x rule pairs have been
		// tried. But choose_source would still
		// remain a hack anyway.
		if (biased_randbool(0.01)) {
			for (const Handle& h : _selected_sources) {
				if (h->isLink()) {
					const HandleSeq& outgoings = h->getOutgoingSet();
					HandleSeq no_free_vars_outgoings;
					// Only add children with no free variables in them
					for (const Handle& h : outgoings)
						if (is_closed(h))
							no_free_vars_outgoings.push_back(h);
					update_potential_sources(no_free_vars_outgoings);
				}
			}
			fc_logger().debug() << (_potential_sources.size() - selsrc_size)
			                    << " sources' children have been added as "
			                    << "potential sources";
		} else {
			fc_logger().debug() << "No added sources, "
			                    << "retry existing sources instead";
		}
	}

	fc_logger().debug() << "Selected sources so far "
	                    << selsrc_size << "/" << _potential_sources.size();

	URECommons urec(_as);
	map<Handle, float> tournament_elem;

	const UnorderedHandleSet& to_select_sources =
		_unselected_sources.empty() ? _potential_sources : _unselected_sources;

	Handle hchosen;
	switch (_ts_mode) {
	case source_selection_mode::TV_FITNESS:
	    for (const Handle& s : to_select_sources)
		    tournament_elem[s] = urec.tv_fitness(s);
	    hchosen = urec.tournament_select(tournament_elem);
	    break;

/*
An attentionbank is needed in order to get the STI...
	case source_selection_mode::STI:
	    for (const Handle& s : to_select_sources)
		    tournament_elem[s] = s->getSTI();
	    hchosen = urec.tournament_select(tournament_elem);
	    break;
*/

	case source_selection_mode::UNIFORM:
		hchosen = rand_element(to_select_sources);
	    break;

	default:
	    throw RuntimeException(TRACE_INFO, "Unknown source selection mode.");
	    break;
	}

	OC_ASSERT(hchosen != Handle::UNDEFINED);
	
	_selected_sources.insert(hchosen);
	_unselected_sources.erase(hchosen);

	return hchosen;
}

Rule ForwardChainer::select_rule(const Handle& hsource)
{
	std::map<const Rule*, float> rule_weight;
	for (const Rule& r : _rules)
		rule_weight[&r] = r.get_weight();

	fc_logger().debug("%d rules to be searched as matched against the source",
	                  rule_weight.size());

	// Select a rule among the admissible rules in the rule-base via stochastic
	// selection, based on the weights of the rules in the current context.
	Rule rule;

	while (not rule_weight.empty()) {
		const Rule *temp = _rec.tournament_select(rule_weight);
		fc_logger().fine("Selected rule %s to match against the source",
		                 temp->get_name().c_str());

		RuleSet unified_rules =
			Rule::strip_typed_substitution(temp->unify_source(hsource));

		if (not unified_rules.empty()) {
			// Randomly select a rule amongst the unified ones
			rule = *std::next(unified_rules.begin(),
			                  randGen().randint(unified_rules.size()));

			fc_logger().debug("Rule %s matched the source",
			                  rule.get_name().c_str());
			break;
		} else {
			fc_logger().debug("Rule %s is not a match. Looking for another rule",
			                  temp->get_name().c_str());
		}

		rule_weight.erase(temp);
	}

	return rule;
};

UnorderedHandleSet ForwardChainer::apply_rule(const Rule& rule)
{
	Handle result = rule.apply(_search_focus_set ? _focus_set_as : _as);

	LAZY_FC_LOG_DEBUG << "Results:" << std::endl
                      << result->toShortString();

	return UnorderedHandleSet(result->getOutgoingSet().begin(),
	                          result->getOutgoingSet().end());
}

void ForwardChainer::validate(const Handle& hsource, const HandleSeq& hfocus_set)
{
    if (hsource == Handle::UNDEFINED)
        throw RuntimeException(TRACE_INFO, "ForwardChainer - Invalid source.");
    // Any other validation here
}

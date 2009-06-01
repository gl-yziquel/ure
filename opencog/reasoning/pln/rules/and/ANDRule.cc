/*
 * Copyright (C) 2002-2007 Novamente LLC
 * Copyright (C) 2008 by Singularity Institute for Artificial Intelligence
 * All Rights Reserved
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

#include <opencog/util/platform.h>
#include "../../PLN.h"

#include "../Rule.h"
#include "../Rules.h"
#include "../../AtomSpaceWrapper.h"
#include "../../PLNatom.h"
#include "../../BackInferenceTreeNode.h"

namespace opencog { namespace pln {

Rule::setOfMPs ANDRule::o2iMetaExtra(meta outh, bool& overrideInputFilter) const
{
    AtomSpaceWrapper *nm = GET_ATW;

    //return Rule::setOfMPs();
    
    ///// Temporarily disabled due to strange bugs!
    
    tree<Vertex>::iterator top = outh->begin();

        if (! (nm->inheritsType(nm->getType(_v2h(*top)), AND_LINK)) ||
            top.number_of_children() <= 2)  
            return Rule::setOfMPs();

        /// This Rule cannot produce nested ANDLinks. Try SimpleANDRule instead.

        for (tree<Vertex>::sibling_iterator j = outh->begin(top); j != outh->end(top); j++)
            if (nm->inheritsType(nm->getType(_v2h(*j)), AND_LINK))
                return Rule::setOfMPs();

        MPs ret;

        std::set<atom,lessatom> query_set;
        for (tree<Vertex>::sibling_iterator i = outh->begin(top); i != outh->end(top); i++)
        {
            tree<Vertex> t_tmp(i);
            query_set.insert(atom(t_tmp, t_tmp.begin()));
        }

        /// Smart lookup begins

        Btr<std::set<pHandle> > sANDLink_set = destTable->getHandleSet(AND_LINK,"");
        std::vector<pHandle> ANDLink_set(sANDLink_set->size());
        std::copy(sANDLink_set->begin(), sANDLink_set->end(), ANDLink_set.begin());

        while (1)
        {
            std::vector<Btr<atom> > max_subset;

            getLargestIntersection2(query_set, ANDLink_set, max_subset);

            if (max_subset.size() > 1)
            {
                for (uint s=0; s < max_subset.size(); s++)
                    query_set.erase(*max_subset[s]);

				ret.push_back(BBvtree(new BoundVTree(atom(AND_LINK, max_subset).makeHandletree(destTable))));

                continue;
            }
            else break;
        }

        /// Add the remaining ones.

		for (std::set<atom, lessatom>::iterator i = query_set.begin(); i != query_set.end(); i++)
			ret.push_back(BBvtree(new BoundVTree(i->makeHandletree(destTable))));

        overrideInputFilter = true;

        
        return makeSingletonSet(ret);
}

/*boost::shared_ptr<set<BoundVertex > > ANDRule::attemptDirectProduction(meta outh);
{
    return attemptDirectANDProduction(destTable, outh, this);
}*/

BoundVertex ANDRule::compute(const std::vector<Vertex>& premiseArray, pHandle CX) const
{
    AtomSpaceWrapper *nm = GET_ATW;
    const int n = premiseArray.size();
  try
  {     
    for (int i=0; i < n; i++)
        if (!_v2h(&premiseArray[i]))
            return Vertex(PHANDLE_UNDEFINED);

    // The items in the the premiseArray are divided into ANDLinks and nodes.
    // Then, the nodes are combined into a single ANDlink by symmetric AND formula,
    // and added to the list of ANDlinks.
    
    int p=0;
    
    
LOG(3, "ANDRule::compute");

    std::set<pHandle> premises, nodes;
    DistinguishNodes(premiseArray, premises, nodes);
LOG(4, "ANDRule::compute");
//  if (!nodes.empty())
//      premises.push_back( computeSymmetric(nodes) );

    for (std::set<pHandle>::iterator j = nodes.begin(); j != nodes.end(); j++) //Nodes included also separately.
    {
        premises.insert(*j);
    }
LOG(4, "ANDRule::compute");
    TruthValue **partialTVs = new TruthValue*[premises.size()];

    std::set<pHandle>::const_iterator i;
    std::set<pHandle> conjunct;
    std::set<TruthValue*> TVowner;

    /// Create the set of elements in the result conjunction
LOG(4, "ANDRule::computeCC");
    for (i = premises.begin(); i != premises.end(); i++)
    {
        std::vector<pHandle> inc2;

        if (nm->isSubType(*i, AND_LINK))
            inc2 = nm->getOutgoing(*i);
        else
            inc2.push_back(*i);

        const std::vector<pHandle>* inc = &inc2;
        
        for (std::vector<pHandle>::const_iterator j = inc->begin(); j != inc->end(); j++)
            if (conjunct.find(*j) == conjunct.end())
                conjunct.insert(*j);
    }
LOG(4, "22 ANDRule::compute");
    /// Loop thru the premises, creating the partialTVs.

    for (p=0, i = premises.begin(); i != premises.end(); i++, p++)
    {
LOG(4, "Q ANDRule::compute");
    /// Put into Di all the elements of the result conjunct not present in the premise #i
        std::set<pHandle> Di;
        for (std::set<pHandle>::const_iterator j = conjunct.begin(); j != conjunct.end(); j++)
        {
            std::vector<pHandle> inc2; // = nm->getOutgoing(*i);

            if (nm->isSubType(*i, AND_LINK))
                inc2 = nm->getOutgoing(*i);
            else
                inc2.push_back(*i);

            std::vector<pHandle>* inc = &inc2;
            if (!vectorHas<pHandle>(*inc, *j))
                Di.insert(*j);
        }
LOG(4, "W ANDRule::compute");
        int Dis = Di.size();
    
        std::set<pHandle> DiSubsets;
        pHandle largest_intersection;

LOG(4,"ANDRule::compute:");

NMPrinter printer(NMP_HANDLE|NMP_TYPE_NAME);
for (std::set<pHandle>::const_iterator di = Di.begin(); di != Di.end(); di++)
    printer.print(*di, 4);
        
LOG(4, "ANDRule:: getLargestIntersection");
        while (getLargestIntersection(Di, premises, largest_intersection))
        {
cprintf(4,"Y ANDRule::compute Di size = %u\n", (uint) Di.size());          
            const std::vector<pHandle> new_elem2 = nm->getOutgoing(largest_intersection);

#ifdef WIN32            
            for (std::vector<pHandle>::const_iterator k = new_elem2.begin(); k != new_elem2.end();)
            {   
                std::vector<pHandle>::const_iterator next_k = k;
                next_k++;
                Di.erase(*k);
                k = next_k;
            }
#else
            std::set<pHandle> new_elemset(new_elem2.begin(), new_elem2.end());
            std::set<pHandle> old_Di = Di;
            Di.clear();
            
            for (std::set<pHandle>::const_iterator d = old_Di.begin();
                d != old_Di.end(); d++)
                if (!STLhas(new_elemset, *d))
                    Di.insert(*d);          
            
/*          std::vector<Handle>::iterator Dii;
            
            *Dii = 0; //old_Di.begin();
            
            set_difference< std::set<Handle>::iterator,
                            std::vector<Handle>::const_iterator,
                            std::set<Handle>::iterator>
                (old_Di.begin(), old_Di.end(),
                 new_elem2.begin(), new_elem2.end(),
                 Dii);*/
#endif          
            DiSubsets.insert(largest_intersection); 
        }

LOG(4, "ANDRule:: getLargestIntersection OK!");
        TruthValue** tvs = new TruthValue*[1 + DiSubsets.size()];
        
        tvs[0] = (TruthValue*) &(GET_ATW->getTV(*i));
    
        int h=0;
        std::set<pHandle>::const_iterator ss;
        for (h = 0, ss = DiSubsets.begin(); ss != DiSubsets.end(); ss++, h++)
            tvs[h+1] = (TruthValue*) &(GET_ATW->getTV(*ss));
LOG(4, "R ANDRule::compute");

/*      if (DiSubsets.size()>0)
        {
            printf("AND ");
            for (set<Handle>::const_iterator z=DiSubsets.begin();z != DiSubsets.end(); z++)
            {
                printf("(");
                for (int y=0; y < nm->getOutgoing(*z).size(); y++)
                    printf("%s ", nm->getName(nm->getOutgoing(*z)[y]).c_str());
                printf(")");
            }
            printf("\n");
        }       
*/
        // Calculate either the ANDNode evaluation or just use the premise strength:

        if (DiSubsets.size()>0)
        {
            partialTVs[p] = fN.compute(tvs, 1+DiSubsets.size());
            TVowner.insert(partialTVs[p]); // TVs to be deleted later
        }
        else
            partialTVs[p] = tvs[0];

//      printf("Part: %f\n", partialTVs[p]->getMean());
        
        delete[] tvs;
    }
    /// Combine the partialTVs
LOG(4, "33 ANDRule::compute");

    TruthValue* retTV = fN.compute(partialTVs, premises.size());

    pHandleSeq outgoing;
    for (std::set<pHandle>::const_iterator c = conjunct.begin(); c != conjunct.end(); c++)
        outgoing.push_back(*c);
LOG(4, "44 ANDRule::compute");
    pHandle ret = destTable->addLink(AND_LINK, outgoing,
                    *retTV,
                    RuleResultFreshness);   
    
LOG(4, "55 ANDRule::compute");
    delete[] partialTVs;
    delete retTV;
    
    for (std::set<TruthValue*>::iterator t= TVowner.begin();
            t != TVowner.end(); t++)
        delete *t;
LOG(3, "ANDRule::compute ok.");		
    
        return Vertex(ret);

    } catch(...) {
        LOG(-10, "Exception in ANDRule::compute");
        return Vertex(PHANDLE_UNDEFINED);
    }
}

}} // namespace opencog { namespace pln {

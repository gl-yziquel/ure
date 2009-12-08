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

StrictImplicationBreakdownRule::StrictImplicationBreakdownRule(iAtomSpaceWrapper *_destTable)
  : Rule(_destTable,false,true,"ModusPonensRule")
{
    inputFilter.push_back(meta(
                               new tree<Vertex>(
                                                mva((pHandle)IMPLICATION_LINK,
                                                    mva((pHandle)ATOM),
                                                    mva((pHandle)ATOM)))
                               ));
    inputFilter.push_back(meta(
                               new tree<Vertex>(       
                                                mva((pHandle)ATOM))
                               ));
}
        
Rule::setOfMPs StrictImplicationBreakdownRule::o2iMetaExtra(meta outh, bool& overrideInputFilter) const
{
    ///haxx:: (restricts internal implications
    
    //      if (inheritsType(nm->getType(v2h(*outh->begin())), IMPLICATION_LINK))
    //          return Rule::setOfMPs();
    
    MPs ret;
    Vertex myvar = CreateVar(destTable);
    
    cprintf(4,"\n\nTo produce\n");
    NMPrinter printer(NMP_HANDLE|NMP_TYPE_NAME);
    printer.print(outh->begin(), 4);
    
    ret.push_back(BBvtree(new BoundVTree(mva((pHandle)IMPLICATION_LINK,
                                             vtree(myvar),*outh))));
    cprintf(4,"Need:\n");
    ret.push_back(BBvtree(new BoundVTree(myvar)));
    printer.print(ret[0]->begin(), 4);
    printer.print(ret[1]->begin(), 4);
    
    cprintf(4,"-----\n");
    
    overrideInputFilter = true;
    
    return makeSingletonSet(ret);
}

BoundVertex StrictImplicationBreakdownRule::compute(const std::vector<Vertex>& premiseArray, pHandle CX) const
{
    AtomSpaceWrapper *nm = GET_ASW;
    assert(validate(premiseArray));
    
    //printTree(premiseArray[0],0,1);
    
    cprintf(3,"StrictImplicationBreakdownRule::compute:");
    
    NMPrinter printer(NMP_ALL);
    for (uint i=0;i<premiseArray.size();i++)
        printer.print(_v2h(premiseArray[i]), 3);
    
    //  vtree   vt1(make_vtree(nm->getOutgoing(v2h(premiseArray[0]),0))),
    //          vt2(make_vtree(v2h(premiseArray[1])));
    
    vtree   vt1(make_vtree(_v2h(premiseArray[0]))),
        vt2(make_vtree(_v2h(premiseArray[1])));
    
    /** haxx:: \todo Temporarily disabled!
        This check does not hold if one of the args
        has been executed but the other one has not.
        Execution here means that Eval(!now) becomes Eval(35353).
        ! is a hacky shorthand for grounded predicates for now.
        
        The real solution will be tointroduce an equality check which considers
        the unexecuted and executed forms equal.
    */
    
#if 0
    
    if (make_vtree(nm->getOutgoing(v2h(premiseArray[0]),0))
        != make_vtree(v2h(premiseArray[1])))
        {
            cprintf(0,"StrictImplicationBreakdownRule args fail:\n");
#if 0
            printTree(v2h(premiseArray[0]),0,0);
            printTree(nm->getOutgoing(v2h(premiseArray[0]),0),0,0);
            printTree(v2h(premiseArray[1]),0,0);
            
            //      rawPrint(premiseArray[0], premiseArray[0].begin(), 0);
            rawPrint(vt1, vt1.begin(), 0);
            rawPrint(vt2, vt2.begin(), 0);
#else 
            printer.print(v2h(premiseArray[0]), -10);
            printer.print(nm->getOutgoing(v2h(premiseArray[0]),0), -10);
            printer.print(v2h(premiseArray[1]), -10);
            
            //        printer.print(premiseArray[0].begin());
            printer.print(vt1.begin(), -10);
            printer.print(vt2.begin(), -10);
#endif
            assert(0);
        }
#endif
    
    std::vector<pHandle> args = GET_ASW->getOutgoing(_v2h(premiseArray[0]));
    Type T = GET_ASW->getType(args[1]);
    std::string pname = nm->getName(args[1]);
    
    TruthValue* tvs[] = {
        (TruthValue*) &(GET_ASW->getTV(_v2h(premiseArray[0]))),
        (TruthValue*) &(GET_ASW->getTV(_v2h(premiseArray[1]))),
        (TruthValue*) &(GET_ASW->getTV(args[1]))
    };
    
    TruthValue* retTV =
        ImplicationBreakdownFormula().compute(tvs, 3);
    
    std::vector<pHandle> new_args = nm->getOutgoing(args[1]);
    
    pHandle ret=PHANDLE_UNDEFINED;
    
    assert (!(GET_ASW->inheritsType(T, NODE)));
    
    ret = destTable->addLink(T, new_args, *retTV, RuleResultFreshness);
    
    delete retTV;
    
    printer.print(ret, 3);
    
    return Vertex(ret);
}
        
}} // namespace opencog { namespace pln {

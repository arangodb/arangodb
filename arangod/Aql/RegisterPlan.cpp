////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "RegisterPlan.h"

#include "Aql/ClusterNodes.h"
#include "Aql/CollectNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/GraphNode.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/SubqueryEndExecutionNode.h"

using namespace arangodb;
using namespace arangodb::aql;

// Requires RegisterPlan to be defined
VarInfo::VarInfo(int depth, RegisterId registerId)
    : depth(depth), registerId(registerId) {
  TRI_ASSERT(registerId < RegisterPlan::MaxRegisterId);
}

// Copy constructor used for a subquery:
RegisterPlan::RegisterPlan(RegisterPlan const& v, unsigned int newdepth)
    : varInfo(v.varInfo),
      nrRegsHere(v.nrRegsHere),
      nrRegs(v.nrRegs),
      subQueryNodes(),
      depth(newdepth + 1),
      totalNrRegs(v.nrRegs[newdepth]),
      me(nullptr) {
  if (depth + 1 < 8) {
    // do a minium initial allocation to avoid frequent reallocations
    nrRegsHere.reserve(8);
    nrRegs.reserve(8);
  }
  nrRegsHere.resize(depth + 1);
  nrRegsHere.back() = 0;
  // create a copy of the last value here
  // this is required because back returns a reference and emplace/push_back may
  // invalidate all references
  nrRegs.resize(depth);
  RegisterId registerId = nrRegs.back();
  nrRegs.emplace_back(registerId);
}

void RegisterPlan::clear() {
  varInfo.clear();
  nrRegsHere.clear();
  nrRegs.clear();
  subQueryNodes.clear();
  depth = 0;
  totalNrRegs = 0;
}

RegisterPlan* RegisterPlan::clone(ExecutionPlan* otherPlan, ExecutionPlan* plan) {
  auto other = std::make_unique<RegisterPlan>();

  other->nrRegsHere = nrRegsHere;
  other->nrRegs = nrRegs;
  other->depth = depth;
  other->totalNrRegs = totalNrRegs;

  other->varInfo = varInfo;

  // No need to clone subQueryNodes because this was only used during
  // the buildup.

  return other.release();
}

void RegisterPlan::after(ExecutionNode* en) {
  switch (en->getType()) {
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::INDEX: {
      depth++;
      nrRegsHere.emplace_back(1);
      // create a copy of the last value here
      // this is required because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = 1 + nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = dynamic_cast<DocumentProducingNode const*>(en);
      if (ep == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "unexpected cast result for DocumentProducingNode");
      }

      varInfo.emplace(ep->outVariable()->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      break;
    }

    case ExecutionNode::ENUMERATE_LIST: {
      depth++;
      nrRegsHere.emplace_back(1);
      // create a copy of the last value here
      // this is required because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = 1 + nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = ExecutionNode::castTo<EnumerateListNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->outVariable()->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      break;
    }

    case ExecutionNode::CALCULATION: {
      nrRegsHere[depth]++;
      nrRegs[depth]++;
      auto ep = ExecutionNode::castTo<CalculationNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->outVariable()->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      break;
    }

    case ExecutionNode::SUBQUERY: {
      nrRegsHere[depth]++;
      nrRegs[depth]++;
      auto ep = ExecutionNode::castTo<SubqueryNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->outVariable()->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      subQueryNodes.emplace_back(en);
      break;
    }

    case ExecutionNode::COLLECT: {
      depth++;
      nrRegsHere.emplace_back(0);
      // create a copy of the last value here
      // this is required because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = ExecutionNode::castTo<CollectNode const*>(en);
      for (auto const& p : ep->groupVariables()) {
        // p is std::pair<Variable const*,Variable const*>
        // and the first is the to be assigned output variable
        // for which we need to create a register in the current
        // frame:
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(p.first->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      for (auto const& p : ep->aggregateVariables()) {
        // p is std::pair<Variable const*,Variable const*>
        // and the first is the to be assigned output variable
        // for which we need to create a register in the current
        // frame:
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(p.first->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      if (ep->hasOutVariable()) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->outVariable()->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::UPSERT: {
      depth++;
      nrRegsHere.emplace_back(0);
      // create a copy of the last value here
      // this is required because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = nrRegs.back();
      nrRegs.emplace_back(registerId);

      auto ep = dynamic_cast<ModificationNode const*>(en);
      if (ep == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "unexpected cast result for ModificationNode");
      }
      if (ep->getOutVariableOld() != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->getOutVariableOld()->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      if (ep->getOutVariableNew() != nullptr) {
        nrRegsHere[depth]++;
        nrRegs[depth]++;
        varInfo.emplace(ep->getOutVariableNew()->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }

      break;
    }

    case ExecutionNode::SORT: {
      // sort sorts in place and does not produce new registers
      break;
    }

    case ExecutionNode::RETURN: {
      // return is special. it produces a result but is the last step in the
      // pipeline
      break;
    }

    case ExecutionNode::SINGLETON:
    case ExecutionNode::FILTER:
    case ExecutionNode::LIMIT:
    case ExecutionNode::SCATTER:
    case ExecutionNode::DISTRIBUTE:
    case ExecutionNode::GATHER:
    case ExecutionNode::REMOTE:
    case ExecutionNode::DISTRIBUTE_CONSUMER:
    case ExecutionNode::NORESULTS: {
      // these node types do not produce any new registers
      break;
    }

    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::K_SHORTEST_PATHS: {
      depth++;
      auto ep = dynamic_cast<GraphNode const*>(en);
      if (ep == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unexpected cast result for GraphNode");
      }
      TRI_ASSERT(ep != nullptr);
      auto vars = ep->getVariablesSetHere();
      nrRegsHere.emplace_back(static_cast<RegisterId>(vars.size()));
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = static_cast<RegisterId>(vars.size() + nrRegs.back());
      nrRegs.emplace_back(registerId);

      for (auto& it : vars) {
        varInfo.emplace(it->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::REMOTESINGLE: {
      depth++;
      auto ep = ExecutionNode::castTo<SingleRemoteOperationNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      auto vars = ep->getVariablesSetHere();
      nrRegsHere.emplace_back(static_cast<RegisterId>(vars.size()));
      // create a copy of the last value here
      // this is requried because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = static_cast<RegisterId>(vars.size() + nrRegs.back());
      nrRegs.emplace_back(registerId);

      for (auto& it : vars) {
        varInfo.emplace(it->id, VarInfo(depth, totalNrRegs));
        totalNrRegs++;
      }
      break;
    }

    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      auto ep = ExecutionNode::castTo<iresearch::IResearchViewNode const*>(en);
      TRI_ASSERT(ep);

      ep->planNodeRegisters(nrRegsHere, nrRegs, varInfo, totalNrRegs, ++depth);
      break;
    }

    case ExecutionNode::SUBQUERY_START: {
      break;
    }

    case ExecutionNode::SUBQUERY_END: {
      nrRegsHere[depth]++;
      nrRegs[depth]++;
      auto ep = ExecutionNode::castTo<SubqueryEndNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->outVariable()->id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      subQueryNodes.emplace_back(en);
      break;
    }
    case ExecutionNode::MATERIALIZE: {
      depth++;
      nrRegsHere.emplace_back(1);
      // create a copy of the last value here
      // this is required because back returns a reference and emplace/push_back
      // may invalidate all references
      RegisterId registerId = nrRegs.back() + 1;
      nrRegs.emplace_back(registerId);
      auto ep = ExecutionNode::castTo<MaterializeNode const*>(en);
      TRI_ASSERT(ep != nullptr);
      varInfo.emplace(ep->outVariable().id, VarInfo(depth, totalNrRegs));
      totalNrRegs++;
      break;
    }
    default: {
      // should not reach this point
      TRI_ASSERT(false);
    }
  }

  en->_depth = depth;
  en->_registerPlan = *me;

  // Now find out which registers ought to be erased after this node:
  if (en->getType() != ExecutionNode::RETURN) {
    // ReturnNodes are special, since they return a single column anyway
    arangodb::HashSet<Variable const*> varsUsedLater = en->getVarsUsedLater();
    arangodb::HashSet<Variable const*> varsUsedHere;
    en->getVariablesUsedHere(varsUsedHere);

    // We need to delete those variables that have been used here but are not
    // used any more later:
    std::unordered_set<RegisterId> regsToClear;

    for (auto const& v : varsUsedHere) {
      auto it = varsUsedLater.find(v);

      if (it == varsUsedLater.end()) {
        auto it2 = varInfo.find(v->id);

        if (it2 == varInfo.end()) {
          // report an error here to prevent crashing
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         std::string("missing variable #") +
                                             std::to_string(v->id) + " (" + v->name +
                                             ") for node #" + std::to_string(en->id()) +
                                             " (" + en->getTypeString() +
                                             ") while planning registers");
        }

        // finally adjust the variable inside the IN calculation
        TRI_ASSERT(it2 != varInfo.end());
        RegisterId r = it2->second.registerId;
        regsToClear.insert(r);
      }
    }
    en->setRegsToClear(std::move(regsToClear));
  }
}

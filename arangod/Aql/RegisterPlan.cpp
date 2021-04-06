////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "RegisterPlan.h"

#include "Aql/ClusterNodes.h"
#include "Aql/CollectNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/GraphNode.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/SubqueryEndExecutionNode.h"
#include "Basics/Exceptions.h"
#include "Containers/Enumerate.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

// Requires RegisterPlan to be defined
VarInfo::VarInfo(unsigned int depth, RegisterId registerId)
    : depth(depth), registerId(registerId) {
  if (!registerId.isValid()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT, 
                                   std::string("too many registers (") + std::to_string(registerId.value()) + ") needed for AQL query");
  }
}

template <typename T>
void RegisterPlanWalkerT<T>::after(T* en) {
  TRI_ASSERT(en != nullptr);

// TODO There are some difficulties with view nodes to resolve before this code
//      can be activated. Also see the comment on assertNoVariablesMissing()
//      below.
//
#if 0
  /*
   * SubqueryEnd does indeed access a variable that is not valid. How can that
   * be? The varsValid stack is already popped for SUBQUERY_END. But
   * varsUsedHere includes the read of the return value from the subquery level.
   */
  if (en->getType() != ExecutionNode::SUBQUERY_END) {
    VarSet varsUsedHere;
    en->getVariablesUsedHere(varsUsedHere);

    VarSet const& varsValid = en->getVarsValid();
    for (auto&& var : varsUsedHere) {
      if (varsValid.find(var) == varsValid.end()) {
        TRI_ASSERT(false);
        /*
         * There is a single IResearch tests that _requires_ this exception
         * to be thrown.
         */
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       en->getTypeString() + " " +
                                           std::to_string(en->id().id()) + " " +
                                           std::string("Variable ") + var->name +
                                           " (" + std::to_string(var->id) +
                                           ") is used before it was set.");
      }
    }
  }
#endif
  
  // will remain constant for the node throughout this function
  std::vector<Variable const*> const varsSetHere = en->getVariablesSetHere();
  VarSet const varsUsedLater = en->getVarsUsedLater();

  bool const mayReuseRegisterImmediately = en->alwaysCopiesRows();

  if (en->isIncreaseDepth()) {
    // isIncreaseDepth => mayReuseRegisterImmediately
    TRI_ASSERT(mayReuseRegisterImmediately);
    plan->increaseDepth();
  }
  
  if (explain) {
    plan->unusedRegsByNode.emplace(en->id(), unusedRegisters);
  }

  TRI_ASSERT(en->getType() != ExecutionNode::SUBQUERY);

  if (en->getType() == ExecutionNode::SUBQUERY_START ||
      en->getType() == ExecutionNode::SUBQUERY_END) {
    // is SQS or SQE => mayReuseRegisterImmediately
    TRI_ASSERT(mayReuseRegisterImmediately);
  }

  /*
   * For passthrough blocks it is better to assign the registers _before_ we calculate
   * which registers have become unused to prevent reusing a input register as output register.
   *
   * This is not the case if the block is not passthrough since in that case the output row
   * is different from the input row.
   */
  auto const planRegistersForCurrentNode = [&](T* en) -> void {
    for (Variable const* v : varsSetHere) {
      TRI_ASSERT(v != nullptr);
      RegisterId regId = plan->registerVariable(v, unusedRegisters.back());
      if (regId.isRegularRegister()) {
        regVarMappingStack.back().operator[](regId) = v;  // overwrite if existing, create if not
      }
    }
  };

  auto const updateRegsToKeep = [this, &varsSetHere](T* en, VarSet const& varsUsedLater,
                                                     VarSet const& varsValid) {
    auto isSetHere = [&](Variable const* var) {
      return std::find(varsSetHere.begin(), varsSetHere.end(), var) !=
             varsSetHere.end();
    };
    auto isUsedLater = [&](Variable const* var) {
      return std::find(varsUsedLater.begin(), varsUsedLater.end(), var) !=
             varsUsedLater.end();
    };

    // items are pushed for each SubqueryStartNode and popped for
    // SubqueryEndNodes. as they come in pairs, the stack should never be empty.
    TRI_ASSERT(!regsToKeepStack.empty());
    regsToKeepStack.back().clear();
    for (auto const var : varsValid) {
      if (var->type() == Variable::Type::Regular && !isSetHere(var) && isUsedLater(var)) {
        auto reg = plan->variableToRegisterId(var);
        regsToKeepStack.back().emplace(reg);
      }
    }
  };

  // TODO This is here to be backwards-compatible with some view tests relying
  //      on this check. When that is cleaned up, uncomment the check further
  //      above instead and remove this.
  auto const assertNoVariablesMissing = [this, &varsUsedLater](T* en) {
    if (en->getType() != ExecutionNode::RETURN) {
      VarSet varsUsedHere;
      en->getVariablesUsedHere(varsUsedHere);
      for (auto const& v : varsUsedHere) {
        auto it = varsUsedLater.find(v);

        if (it == varsUsedLater.end()) {
          auto it2 = plan->varInfo.find(v->id);

          if (it2 == plan->varInfo.end()) {
            // report an error here to prevent crashing
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                           std::string("missing variable #") +
                                               std::to_string(v->id) + " (" +
                                               v->name + ") for node #" +
                                               std::to_string(en->id().id()) +
                                               " (" + en->getTypeString() +
                                               ") while planning registers");
          }
        }
      }
    }
  };

  auto const calculateRegistersToReuse = [](VarSet const& varsUsedLater,
                                            RegVarMap const& regVarMap) -> RegIdSet {
    RegIdSet regsToReuse;
    for (auto& [regId, variable] : regVarMap) {
      TRI_ASSERT(regId.isRegularRegister());
      TRI_ASSERT(variable->type() == Variable::Type::Regular);
      if (!varsUsedLater.contains(variable)) {
        regsToReuse.emplace(regId);
      }
    }

    return regsToReuse;
  };


  RegIdSet regsToClear;
  auto const updateRegistersToClearAndReuse = [&](bool insertIntoRegsToClear) {
    if (en->getType() != ExecutionNode::RETURN) {
      assertNoVariablesMissing(en);
      auto regsToReuse =
          calculateRegistersToReuse(varsUsedLater, regVarMappingStack.back());
      for (auto const& reg : regsToReuse) {
        TRI_ASSERT(reg.isRegularRegister())
        regVarMappingStack.back().erase(reg);
      }

      unusedRegisters.back().insert(regsToReuse.begin(), regsToReuse.end());
      if (insertIntoRegsToClear) {
        regsToClear.insert(regsToReuse.begin(), regsToReuse.end());
      }
    }
  };

  if (!mayReuseRegisterImmediately) {
    planRegistersForCurrentNode(en);
  }

  switch (en->getType()) {
    case ExecutionNode::SUBQUERY_START: {
      previousSubqueryNrRegs.emplace(plan->nrRegs.back());

      auto const& varsValid = en->getVarsValidStack();
      auto const& varsUsedLaterStack = en->getVarsUsedLaterStack();

      auto varMap = regVarMappingStack.back();
      regVarMappingStack.push_back(std::move(varMap));

      size_t const stackSize = varsUsedLaterStack.size();
      TRI_ASSERT(varsValid.size() == stackSize);
      TRI_ASSERT(regVarMappingStack.size() == stackSize);

      TRI_ASSERT(varsValid.size() > 1);
      TRI_ASSERT(varsUsedLaterStack.size() > 1);
      auto reuseOuter = calculateRegistersToReuse(varsUsedLaterStack[stackSize - 2],
                                                  regVarMappingStack[stackSize - 2]);
      auto reuseInner = calculateRegistersToReuse(varsUsedLaterStack[stackSize - 1],
                                                  regVarMappingStack[stackSize - 1]);

      auto topUnused = unusedRegisters.back();
      unusedRegisters.emplace_back(std::move(topUnused));

      TRI_ASSERT(unusedRegisters.size() == stackSize);
      unusedRegisters[stackSize - 1].insert(reuseInner.begin(), reuseInner.end());
      unusedRegisters[stackSize - 2].insert(reuseOuter.begin(), reuseOuter.end());

      if (explain) {
        plan->regVarMapStackByNode.emplace(en->id(), regVarMappingStack);
      }

      for (auto const& reg : reuseInner) {
        regVarMappingStack[stackSize - 1].erase(reg);
      }
      for (auto const& reg : reuseOuter) {
        regVarMappingStack[stackSize - 2].erase(reg);
      }

      updateRegsToKeep(en, varsUsedLaterStack[varsValid.size() - 2],
                       varsValid[varsValid.size() - 2]);  // subquery start has to update both levels of regs to keep
      regsToKeepStack.emplace_back();

    } break;
    case ExecutionNode::SUBQUERY_END: {
      unusedRegisters.pop_back();
      regsToKeepStack.pop_back();
      regVarMappingStack.pop_back();
      // This must have added a section, otherwise we would decrease the
      // number of registers available inside the subquery.
      TRI_ASSERT(en->isIncreaseDepth());
      // We must plan the registers after this, so newly added registers are
      // based upon this nrRegs.
      TRI_ASSERT(mayReuseRegisterImmediately);
      plan->nrRegs.back() = previousSubqueryNrRegs.top();
      previousSubqueryNrRegs.pop();

      if (explain) {
        plan->regVarMapStackByNode.emplace(en->id(), regVarMappingStack);
      }
    } break;
    default: {
      updateRegistersToClearAndReuse(!mayReuseRegisterImmediately);

      if (explain) {
        plan->regVarMapStackByNode.emplace(en->id(), regVarMappingStack);
      }
    } break;
  }

  // we can reuse all registers that belong to variables
  // that are not in varsUsedLater and varsUsedHere
  if (mayReuseRegisterImmediately) {
    planRegistersForCurrentNode(en);
    updateRegistersToClearAndReuse(true);
  }

  updateRegsToKeep(en, varsUsedLater, en->getVarsValid());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto actual = plan->calcRegsToKeep(en->getVarsUsedLaterStack(), en->getVarsValidStack(), varsSetHere);
  TRI_ASSERT(regsToKeepStack == actual);

  TRI_ASSERT(!plan->nrRegs.empty());
  for (auto regId: regsToClear) {
    TRI_ASSERT(regId < plan->nrRegs.back());
  }

#endif
  
  // We need to delete those variables that have been used here but are
  // not used any more later:
  en->setRegsToClear(std::move(regsToClear));
  en->setRegsToKeep(regsToKeepStack);
  en->_depth = plan->depth;
  en->_registerPlan = plan;
}

template <typename T>
RegisterPlanT<T>::RegisterPlanT() 
    : depth(0) {
  nrRegs.reserve(8);
  nrRegs.emplace_back(0);
}

template <typename T>
RegisterPlanT<T>::RegisterPlanT(VPackSlice slice, unsigned int depth)
    : depth(depth) {
  VPackSlice varInfoList = slice.get("varInfoList");
  if (!varInfoList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "\"varInfoList\" attribute needs to be an array");
  }

  varInfo.reserve(varInfoList.length());

  for (VPackSlice it : VPackArrayIterator(varInfoList)) {
    if (!it.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "\"varInfoList\" item needs to be an object");
    }
    auto variableId = it.get("VariableId").getNumericValue<VariableId>();
    auto registerId = RegisterId::fromUInt32(it.get("RegisterId").getNumericValue<uint32_t>());
    auto depthParam = it.get("depth").getNumericValue<unsigned int>();

    varInfo.try_emplace(variableId, VarInfo(depthParam, registerId));
  }

  VPackSlice nrRegsList = slice.get("nrRegs");
  if (!nrRegsList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "\"nrRegs\" attribute needs to be an array");
  }
  VPackSlice nrConstRegsSlice = slice.get("nrConstRegs");
  if (nrConstRegsSlice.isInteger()) {
    nrConstRegs = static_cast<RegisterCount>(nrConstRegsSlice.getIntUnchecked());
  }

  nrRegs.reserve(nrRegsList.length());
  for (VPackSlice it : VPackArrayIterator(nrRegsList)) {
    nrRegs.emplace_back(it.getNumericValue<RegisterCount>());
  }
}

template <typename T>
auto RegisterPlanT<T>::clone() -> std::shared_ptr<RegisterPlanT> {
  auto other = std::make_shared<RegisterPlanT>();

  other->nrRegs = nrRegs;
  other->depth = depth;
  other->varInfo = varInfo;

  return other;
}

template <typename T>
void RegisterPlanT<T>::increaseDepth() {
  ++depth;

  // create a copy of the last value here
  // this is required because back returns a reference and emplace/push_back
  // may invalidate all references
  auto regCount = nrRegs.back();
  nrRegs.emplace_back(regCount);
}

template <typename T>
void RegisterPlanT<T>::shrink(T* start) {
  // remove all registers greater than maxRegister from the ExecutionNodes 
  // starting at current that have the same depth as current
  auto removeRegistersGreaterThan = [](T* current, RegisterId maxRegister) {
    auto depth = current->getDepth();
    while (current != nullptr && current->getDepth() == depth) {
      current->removeRegistersGreaterThan(maxRegister);
      current = current->getFirstDependency();
    }
  };

  // scan the execution plan upwards, starting at the root node, until
  // we either find the end of the plan or a change in the node's depth.
  // while scanning the ExecutionNodes of a certain depth, we keep track 
  // of the maximum register id they use.
  // when we are at the end of the plan or at a depth change, we know which
  // is the maximum register id used by the depth. if it is different from
  // the already calculated number of registers for this depth, we walk over
  // all nodes in that depth again and remove the superfluous registers from
  // them.
  
  auto maxUsedRegister = [this](auto const& container, RegisterId maxRegisterId) -> RegisterId {
    for (auto const& v : container) {
      auto it = varInfo.find(v->id);
      if (it != varInfo.end()) {
        RegisterId rv = it->second.registerId;
        if (rv.isRegularRegister()) {
          maxRegisterId = std::max(rv, maxRegisterId);
        }
      }
    }
    return maxRegisterId;
  };
  
  // max RegisterId used by nodes on the current depth
  RegisterId maxRegisterId(0);
  
  // node at which the current depth starts
  T* depthStart = start;

  // loop node pointer, will be modified while iterating
  T* current = start;
  
  // currently used variables, outside of loop because the set is recycled
  VarSet varsUsedHere;

  while (current != nullptr) {
    // take the node's used registers into account
    // the maximum used RegisterId for the current depth is tracked in
    // maxRegisterId
    maxRegisterId = maxUsedRegister(current->getVariablesSetHere(), maxRegisterId);
    for (auto const& regsToKeep : current->getRegsToKeepStack()) {
      for (auto const& regId : regsToKeep) {
        maxRegisterId = std::max(regId, maxRegisterId);
      }
    }

    auto depth = current->getDepth();
    T* previous = current->getFirstDependency();
    // check if from the next node onwards there will be a depth change, or if 
    // we are at the end of the plan
    bool const depthChange = (previous == nullptr || previous->getDepth() != depth);

    if (depthChange) {
      auto neededRegisters = static_cast<RegisterCount>(maxRegisterId.value() + 1);
      if (nrRegs[depth] > neededRegisters) {
        // the current RegisterPlan has superfluous registers planned for this 
        // depth. so let's put in some more effort and remove them.
        nrRegs[depth] = neededRegisters;
        // remove the superfluous registers from all the nodes starting at the
        // start node of our current depth...
        removeRegistersGreaterThan(depthStart, maxRegisterId);
      }
      
      // new depth starting, so update the start node for the depth
      depthStart = previous;
      maxRegisterId = RegisterId(0);
    }
    
    // walk upwards
    current = previous;
  }
}

template <typename T>
RegisterId RegisterPlanT<T>::addRegister() {
  return RegisterId(nrRegs[depth]++);
}

template <typename T>
RegisterId RegisterPlanT<T>::registerVariable(Variable const* v,
                                              std::set<RegisterId>& unusedRegisters) {

  RegisterId regId;
  if (v->type() == Variable::Type::Const) {
    regId = RegisterId::makeConst(nrConstRegs++);
  } else if (unusedRegisters.empty()) {
    regId = addRegister();
  } else {
    auto iter = unusedRegisters.begin();
    regId = *iter;
    unusedRegisters.erase(iter);
  }
  TRI_ASSERT(regId.isConstRegister() == (v->type() == Variable::Type::Const));

  bool inserted;
  std::tie(std::ignore, inserted) = varInfo.try_emplace(v->id, VarInfo(depth, regId));
  TRI_ASSERT(inserted);
  if (!inserted) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string("duplicate register assignment for variable " + v->name + " #") +
            std::to_string(v->id) + " while planning registers");
  }

  return regId;
}

template <typename T>
void RegisterPlanT<T>::toVelocyPackEmpty(VPackBuilder& builder) {
  builder.add(VPackValue("varInfoList"));
  { VPackArrayBuilder guard(&builder); }
  builder.add(VPackValue("nrRegs"));
  { VPackArrayBuilder guard(&builder); }
}

template <typename T>
void RegisterPlanT<T>::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  builder.add(VPackValue("varInfoList"));
  {
    VPackArrayBuilder guard(&builder);
    for (auto const& oneVarInfo : varInfo) {
      VPackObjectBuilder guardInner(&builder);
      builder.add("VariableId", VPackValue(oneVarInfo.first));
      builder.add("depth", VPackValue(oneVarInfo.second.depth));
      builder.add("RegisterId", VPackValue(oneVarInfo.second.registerId.toUInt32()));
    }
  }

  builder.add(VPackValue("nrRegs"));
  {
    VPackArrayBuilder guard(&builder);
    for (auto const& oneRegisterID : nrRegs) {
      builder.add(VPackValue(oneRegisterID));
    }
  }

  builder.add("nrConstRegs", VPackValue(nrConstRegs));
}

template <typename T>
auto RegisterPlanT<T>::calcRegsToKeep(VarSetStack const& varsUsedLaterStack,
                                      VarSetStack const& varsValidStack,
                                      std::vector<Variable const*> const& varsSetHere) const
    -> RegIdSetStack {
  RegIdSetStack regsToKeepStack;
  regsToKeepStack.reserve(varsUsedLaterStack.size());

  TRI_ASSERT(varsValidStack.size() == varsUsedLaterStack.size());

  for (auto const [idx, stackEntry] : enumerate(varsValidStack)) {
    auto& regsToKeep = regsToKeepStack.emplace_back();
    auto const& varsUsedLater = varsUsedLaterStack[idx];

    for (auto const var : stackEntry) {
      if (var->type() != Variable::Type::Regular) {
        continue;
      }
      auto reg = variableToRegisterId(var);
      TRI_ASSERT(reg.isRegularRegister());

      bool isUsedLater = std::find(varsUsedLater.begin(), varsUsedLater.end(), var) !=
                         varsUsedLater.end();
      if (isUsedLater) {
        bool isSetHere = std::find(varsSetHere.begin(), varsSetHere.end(), var) !=
                         varsSetHere.end();
        if (!isSetHere) {
          regsToKeep.emplace(reg);
        }
      }
    }
  }

  return regsToKeepStack;
}

template <typename T>
auto RegisterPlanT<T>::variableToRegisterId(Variable const* variable) const -> RegisterId {
  TRI_ASSERT(variable != nullptr);
  auto it = varInfo.find(variable->id);
  TRI_ASSERT(it != varInfo.end());
  RegisterId rv = it->second.registerId;
  TRI_ASSERT(rv.isValid());
  return rv;
}

template <typename T>
auto RegisterPlanT<T>::variableToOptionalRegisterId(VariableId varId) const -> RegisterId {
  auto it = varInfo.find(varId);
  if (it != varInfo.end()) {
    return it->second.registerId;
  }
  return RegisterId{RegisterId::maxRegisterId};
}

template <typename T>
std::ostream& aql::operator<<(std::ostream& os, const RegisterPlanT<T>& r) {
  // level -> variable, info
  std::map<unsigned int, std::map<VariableId, VarInfo>> frames;

  for (auto [id, info] : r.varInfo) {
    frames[info.depth][id] = info;
  }

  for (auto [depth, vars] : frames) {
    os << "depth " << depth << std::endl;
    os << "------------------------------------" << std::endl;

    for (auto [id, info] : vars) {
      os << "id = " << id << " register = " << info.registerId.value() << std::endl;
    }
  }
  return os;
}

template struct aql::RegisterPlanT<ExecutionNode>;
template struct aql::RegisterPlanWalkerT<ExecutionNode>;
template std::ostream& aql::operator<<<ExecutionNode>(std::ostream& os,
                                                      const RegisterPlanT<ExecutionNode>& r);

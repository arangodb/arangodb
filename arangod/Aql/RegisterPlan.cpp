////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
  if (registerId >= RegisterPlan::MaxRegisterId) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT, 
                                   std::string("too many registers (") + std::to_string(RegisterPlan::MaxRegisterId) + ") needed for AQL query");
  }
  TRI_ASSERT(registerId < RegisterPlanT<ExecutionNode>::MaxRegisterId);
}

template <typename T>
void RegisterPlanWalkerT<T>::after(T* en) {
  TRI_ASSERT(en != nullptr);

  if (explain) {
    plan->unusedRegsByNode.emplace(en->id(), unusedRegisters);
  }

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

  bool const mayReuseRegisterImmediately = en->alwaysCopiesRows();

  if (en->isIncreaseDepth()) {
    // isIncreaseDepth => mayReuseRegisterImmediately
    TRI_ASSERT(mayReuseRegisterImmediately);
    plan->increaseDepth();
  }

  if (en->getType() == ExecutionNode::SUBQUERY) {
    plan->addSubqueryNode(en);
  }

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
    auto const& varsSetHere = en->getVariablesSetHere();
    for (Variable const* v : varsSetHere) {
      TRI_ASSERT(v != nullptr);
      RegisterId regId = plan->registerVariable(v, unusedRegisters.back());
      regVarMappingStack.back().operator[](regId) = v;  // overwrite if existing, create if not
    }
  };

  auto const updateRegsToKeep = [this](T* en, VarSet const& varsUsedLater,
                                       VarSet const& varsValid) {
    auto const& varsSetHere = en->getVariablesSetHere();

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
      if (!isSetHere(var) && isUsedLater(var)) {
        auto reg = plan->variableToRegisterId(var);
        regsToKeepStack.back().emplace(reg);
      }
    }
  };

  // TODO This is here to be backwards-compatible with some view tests relying
  //      on this check. When that is cleaned up, uncomment the check further
  //      above instead and remove this.
  auto const assertNoVariablesMissing = [this](T* en) {
    if (en->getType() != ExecutionNode::RETURN) {
      auto const& varsUsedLater = en->getVarsUsedLaterStack().back();
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
      if (!varsUsedLater.contains(variable)) {
        regsToReuse.emplace(regId);
      }
    }

    return regsToReuse;
  };


  RegIdSet regsToClear;
  auto const updateRegistersToClear = [&]() {
    // IMPORTANT NOTE:
    // Note that in case of mayReuseRegisterImmediately, these registers can
    // be reused, but are *still* set in regsToClear! This is *only* okay
    // because regsToClear is only ever used in passthrough-blocks, but
    // nodes that can create those may *not* reuse registers immediately.
    if (en->getType() != ExecutionNode::RETURN) {
      auto const& varsUsedLater = en->getVarsUsedLater();
      assertNoVariablesMissing(en);
      auto regsToReuse =
          calculateRegistersToReuse(varsUsedLater, regVarMappingStack.back());
      for (auto const& reg : regsToReuse) {
        regVarMappingStack.back().erase(reg);
      }

      unusedRegisters.back().insert(regsToReuse.begin(), regsToReuse.end());
      regsToClear.insert(regsToReuse.begin(), regsToReuse.end());
    }

  };

  auto const updateRegistersToReuse = [&]() {
    if (en->getType() != ExecutionNode::RETURN) {
      auto const& varsUsedLater = en->getVarsUsedLater();
      assertNoVariablesMissing(en);
      auto regsToReuse =
          calculateRegistersToReuse(varsUsedLater, regVarMappingStack.back());
      for (auto const& reg : regsToReuse) {
        regVarMappingStack.back().erase(reg);
      }

      unusedRegisters.back().insert(regsToReuse.begin(), regsToReuse.end());
    }

  };

  if (!mayReuseRegisterImmediately) {
    planRegistersForCurrentNode(en);
  }

  switch (en->getType()) {
    case ExecutionNode::SUBQUERY_START: {
      previousSubqueryNrRegs.emplace(plan->nrRegs.back());

      auto const& varsValid = en->getVarsValidStack();
      auto const& varsUsedLater = en->getVarsUsedLaterStack();

      auto varMap = regVarMappingStack.back();
      regVarMappingStack.push_back(std::move(varMap));

      size_t const stack_size = varsUsedLater.size();
      TRI_ASSERT(varsValid.size() == stack_size);
      TRI_ASSERT(regVarMappingStack.size() == stack_size);

      TRI_ASSERT(varsValid.size() > 1);
      TRI_ASSERT(varsUsedLater.size() > 1);
      auto reuseOuter = calculateRegistersToReuse(varsUsedLater[stack_size - 2],
                                                  regVarMappingStack[stack_size - 2]);
      auto reuseInner = calculateRegistersToReuse(varsUsedLater[stack_size - 1],
                                                  regVarMappingStack[stack_size - 1]);

      auto topUnused = unusedRegisters.back();
      unusedRegisters.emplace_back(std::move(topUnused));

      TRI_ASSERT(unusedRegisters.size() == stack_size);
      unusedRegisters[stack_size - 1].insert(reuseInner.begin(), reuseInner.end());
      unusedRegisters[stack_size - 2].insert(reuseOuter.begin(), reuseOuter.end());

      if (explain) {
        plan->regVarMapStackByNode.emplace(en->id(), regVarMappingStack);
      }

      for (auto const& reg : reuseInner) {
        regVarMappingStack[stack_size - 1].erase(reg);
      }
      for (auto const& reg : reuseOuter) {
        regVarMappingStack[stack_size - 2].erase(reg);
      }

      updateRegsToKeep(en, varsUsedLater[varsValid.size() - 2],
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
      if (mayReuseRegisterImmediately) {
        updateRegistersToReuse();
      } else {
        updateRegistersToClear();
      }

      if (explain) {
        plan->regVarMapStackByNode.emplace(en->id(), regVarMappingStack);
      }
    } break;
  }

  // we can reuse all registers that belong to variables
  // that are not in varsUsedLater and varsUsedHere
  if (mayReuseRegisterImmediately) {
    planRegistersForCurrentNode(en);
    updateRegistersToClear();
  }

  updateRegsToKeep(en, en->getVarsUsedLater(), en->getVarsValid());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto actual = plan->calcRegsToKeep(en->getVarsUsedLaterStack(), en->getVarsValidStack(),
                                     en->getVariablesSetHere());
  TRI_ASSERT(regsToKeepStack == actual);
#endif

  // We need to delete those variables that have been used here but are
  // not used any more later:
  en->setRegsToClear(std::move(regsToClear));
  en->setRegsToKeep(regsToKeepStack);
  en->_depth = plan->depth;
  en->_registerPlan = plan;
}

template <typename T>
RegisterPlanT<T>::RegisterPlanT() : depth(0) {
  nrRegs.reserve(8);
  nrRegs.emplace_back(0);
}

// Copy constructor used for a subquery:
template <typename T>
RegisterPlanT<T>::RegisterPlanT(RegisterPlan const& v, unsigned int newdepth)
    : varInfo(v.varInfo), nrRegs(v.nrRegs), subQueryNodes(), depth(newdepth + 1) {
  if (depth + 1 < 8) {
    // do a minium initial allocation to avoid frequent reallocations
    nrRegs.reserve(8);
  }
  // create a copy of the last value here
  // this is required because back returns a reference and emplace/push_back may
  // invalidate all references
  nrRegs.resize(depth);
  auto regCount = nrRegs.back();
  nrRegs.emplace_back(regCount);
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
    auto registerId = it.get("RegisterId").getNumericValue<RegisterId>();
    auto depthParam = it.get("depth").getNumericValue<unsigned int>();

    varInfo.try_emplace(variableId, VarInfo(depthParam, registerId));
  }

  VPackSlice nrRegsList = slice.get("nrRegs");
  if (!nrRegsList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "\"nrRegs\" attribute needs to be an array");
  }

  nrRegs.reserve(nrRegsList.length());
  for (VPackSlice it : VPackArrayIterator(nrRegsList)) {
    nrRegs.emplace_back(it.getNumericValue<RegisterId>());
  }
}

template <typename T>
auto RegisterPlanT<T>::clone() -> std::shared_ptr<RegisterPlanT> {
  auto other = std::make_shared<RegisterPlanT>();

  other->nrRegs = nrRegs;
  other->depth = depth;
  other->varInfo = varInfo;

  // No need to clone subQueryNodes because this was only used during
  // the buildup.

  return other;
}

template <typename T>
void RegisterPlanT<T>::increaseDepth() {
  depth++;
  // create a copy of the last value here
  // this is required because back returns a reference and emplace/push_back
  // may invalidate all references
  auto regCount = nrRegs.back();
  nrRegs.emplace_back(regCount);
}

template <typename T>
RegisterId RegisterPlanT<T>::addRegister() {
  return static_cast<RegisterId>(nrRegs[depth]++);
}

template <typename T>
RegisterId RegisterPlanT<T>::registerVariable(Variable const* v,
                                              std::set<RegisterId>& unusedRegisters) {
  RegisterId regId;

  if (unusedRegisters.empty()) {
    regId = addRegister();
  } else {
    auto iter = unusedRegisters.begin();
    regId = *iter;
    unusedRegisters.erase(iter);
  }

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
  builder.add("totalNrRegs", VPackValue(0));
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
      builder.add("RegisterId", VPackValue(oneVarInfo.second.registerId));
    }
  }

  builder.add(VPackValue("nrRegs"));
  {
    VPackArrayBuilder guard(&builder);
    for (auto const& oneRegisterID : nrRegs) {
      builder.add(VPackValue(oneRegisterID));
    }
  }

  // nrRegsHere is not used anymore and is intentionally left empty
  // can be removed in ArangoDB 3.8
  builder.add(VPackValue("nrRegsHere"));
  { VPackArrayBuilder guard(&builder); }

  // totalNrRegs is not used anymore and is intentionally left empty
  // can be removed in ArangoDB 3.8
  builder.add("totalNrRegs", VPackSlice::noneSlice());
}

template <typename T>
void RegisterPlanT<T>::addSubqueryNode(T* subquery) {
  subQueryNodes.emplace_back(subquery);
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
      auto reg = variableToRegisterId(var);

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
  TRI_ASSERT(rv < RegisterPlan::MaxRegisterId);
  return rv;
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
      os << "id = " << id << " register = " << info.registerId << std::endl;
    }
  }
  return os;
}

template struct aql::RegisterPlanT<ExecutionNode>;
template struct aql::RegisterPlanWalkerT<ExecutionNode>;
template std::ostream& aql::operator<<<ExecutionNode>(std::ostream& os,
                                                      const RegisterPlanT<ExecutionNode>& r);

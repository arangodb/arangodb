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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_REGISTER_PLAN_H
#define ARANGOD_AQL_REGISTER_PLAN_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"
#include "Aql/types.h"
#include "Basics/Common.h"

#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
}  // namespace arangodb

namespace arangodb::aql {

class ExecutionNode;
class ExecutionPlan;
struct Variable;

/// @brief static analysis, walker class and information collector
struct VarInfo {
  unsigned int depth;
  RegisterId registerId;

  VarInfo() = default;
  VarInfo(unsigned int depth, RegisterId registerId);
};

template <typename T>
struct RegisterPlanT;

using RegVarMap = std::unordered_map<RegisterId, Variable const*>;
using RegVarMapStack = std::vector<RegVarMap>;

/// There are still some improvements that can be done to the RegisterPlanWalker
/// to produce better plans.
/// The most important point is that registersToClear are currently used to find
/// unused registers that can be reused. That is correct, but does not include
/// all cases that can be reused. For example:
///  - A register that is written in one node and never used later will not be
///    found.
///  - When a spliced subquery starts (at a SubqueryStartNode), two things are
///    missed:
///    1) registers that are used after the subquery, but not inside, will not
///       be marked as unused registers inside the subquery.
///    2) registers that are used inside the subquery, but not after it, are not
///       marked as unused registers outside the subquery (i.e. on the stack
///       level below it). It would of course suffice when this would be done
///       at the SubqueryEndNode.
template <typename T>
struct RegisterPlanWalkerT final : public WalkerWorker<T, WalkerUniqueness::NonUnique> {
  using RegisterPlan = RegisterPlanT<T>;

  explicit RegisterPlanWalkerT(std::shared_ptr<RegisterPlan> plan,
                               ExplainRegisterPlan explainRegisterPlan = ExplainRegisterPlan::No)
      : plan(std::move(plan)),
        explain(explainRegisterPlan == ExplainRegisterPlan::Yes) {}
  virtual ~RegisterPlanWalkerT() noexcept = default;

  void after(T* eb) override final;
  bool enterSubquery(T*, T*) override final {
    return false;  // do not walk into subquery
  }

  using RegCountStack = std::stack<RegisterCount, std::vector<RegisterCount>>;

  RegIdOrderedSetStack unusedRegisters{{}};
  RegIdSetStack regsToKeepStack{{}};
  std::shared_ptr<RegisterPlan> plan;
  bool explain = false;
  RegCountStack previousSubqueryNrRegs{};

  RegVarMapStack regVarMappingStack{{}};
};

template <typename T>
struct RegisterPlanT final : public std::enable_shared_from_this<RegisterPlanT<T>> {
  friend struct RegisterPlanWalkerT<T>;
  // The following are collected for global usage in the ExecutionBlock,
  // although they are stored here in the node:

  // map VariableIds to their depth and registerId:
  std::unordered_map<VariableId, VarInfo> varInfo;

  // number of variables in this and all outer frames together,
  // the entry with index i here is always the sum of all values
  // in nrRegsHere from index 0 to i (inclusively) and the two
  // have the same length:
  std::vector<RegisterCount> nrRegs;

  RegisterCount nrConstRegs = 0;

  /// @brief maximum register id that can be assigned, plus one.
  /// this is used for assertions
  static constexpr RegisterId MaxRegisterId = RegisterId(RegisterId::maxRegisterId);
  // TODO - remove MaxRegisterId in favor of RegisterId::maxRegisterId

  /// @brief Only used when the register plan is being explained
  std::map<ExecutionNodeId, RegIdOrderedSetStack> unusedRegsByNode;
  /// @brief Only used when the register plan is being explained
  std::map<ExecutionNodeId, RegVarMapStack> regVarMapStackByNode;

 public:
  RegisterPlanT();
  RegisterPlanT(arangodb::velocypack::Slice slice, unsigned int depth);
  ~RegisterPlanT() = default;

  std::shared_ptr<RegisterPlanT> clone();

  RegisterId registerVariable(Variable const* v, std::set<RegisterId>& unusedRegisters);
  void increaseDepth();
  auto addRegister() -> RegisterId;
  void shrink(T* start);

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  static void toVelocyPackEmpty(arangodb::velocypack::Builder& builder);

  auto variableToRegisterId(Variable const* variable) const -> RegisterId;
  auto variableToOptionalRegisterId(VariableId varId) const -> RegisterId;

  auto calcRegsToKeep(VarSetStack const& varsUsedLaterStack, VarSetStack const& varsValidStack,
                      std::vector<Variable const*> const& varsSetHere) const -> RegIdSetStack;

 private:
  unsigned int depth;
};

template <typename T>
std::ostream& operator<<(std::ostream& os, RegisterPlanT<T> const& r);

}  // namespace arangodb::aql

#endif

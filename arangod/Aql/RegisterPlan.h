////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionNodeId.h"
#include "Aql/RegisterId.h"
#include "Aql/RegisterResolver.h"
#include "Aql/WalkerWorker.h"
#include "Aql/types.h"
#include "Basics/Exceptions.h"

#include <iosfwd>
#include <map>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

#ifdef ARANGODB_USE_GOOGLE_TESTS
namespace arangodb::tests::aql {
struct ExecutionNodeMock;
}
#endif

namespace arangodb::aql {

class ExecutionNode;
struct Variable;

template<typename T>
struct RegisterPlanT;

using RegVarMap = std::unordered_map<RegisterId, Variable const*>;
using RegVarMapStack = std::vector<RegVarMap>;

/// Assigns a register to each variable while it walks the plan from the
/// singleton to the root. A register becomes free when no later node uses its
/// variable, and the walker gives the lowest free register to the next
/// variable. Each spliced subquery level has its own set of free registers.
///
/// A node that passes its input block through (alwaysCopiesRows() is false)
/// writes its outputs into that block. Thus it gets its output registers
/// before the walker frees the registers of the variables that die at this
/// node, because an output must not overwrite an input that the node still
/// reads. A node that builds new rows frees those registers first, so its
/// outputs can reuse them.
///
/// A variable keeps its register for the whole query. If a variable stays live
/// into a later depth, its register index stays the same there, and shrink()
/// can only remove unused registers above the highest index in use.
template<typename T>
struct RegisterPlanWalkerT final
    : public WalkerWorker<T, WalkerUniqueness::NonUnique> {
  using RegisterPlan = RegisterPlanT<T>;

  explicit RegisterPlanWalkerT(
      std::shared_ptr<RegisterPlan> plan,
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

template<typename T>
struct RegisterPlanT final
    : public std::enable_shared_from_this<RegisterPlanT<T>> {
  friend struct RegisterPlanWalkerT<T>;
  // The following are collected for global usage in the ExecutionBlock,
  // although they are stored here in the node:

  // map VariableIds to their depth and registerId:
  aql::VarInfoMap varInfo;

  // number of variables in this and all outer frames together,
  // the entry with index i here is always the sum of all values
  // in nrRegsHere from index 0 to i (inclusively) and the two
  // have the same length:
  std::vector<RegisterCount> nrRegs;

  RegisterCount nrConstRegs = 0;

  /// @brief maximum register id that can be assigned, plus one.
  /// this is used for assertions
  static constexpr RegisterId MaxRegisterId =
      RegisterId(RegisterId::maxRegisterId);
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

  RegisterId registerVariable(Variable const* v,
                              std::set<RegisterId>& unusedRegisters);
  void increaseDepth();
  auto addRegister() -> RegisterId;
  void shrink(T* start);

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  static void toVelocyPackEmpty(arangodb::velocypack::Builder& builder);

  /// @brief a resolver bound to one depth, for code that has to resolve
  /// variables later than register planning (e.g. during execution).
  auto resolverForDepth(unsigned int depth) const -> RegisterResolver;

  /// @brief Register of a constant or bind-parameter variable. Const registers
  /// live in one query-global block rather than in the per-depth item blocks,
  /// so this lookup is deliberately depth-independent. Returns an invalid
  /// RegisterId when the variable was never assigned a register.
  auto constVariableToRegisterId(VariableId varId) const -> RegisterId;

  auto calcRegsToKeep(VarSetStack const& varsUsedLaterStack,
                      VarSetStack const& varsValidStack,
                      std::vector<Variable const*> const& varsSetHere) const
      -> RegIdSetStack;

 private:
  /// @brief depth-agnostic lookups, used during register planning itself
  auto variableToRegisterId(Variable const* variable) const -> RegisterId;
  auto variableToOptionalRegisterId(VariableId varId) const -> RegisterId;

  unsigned int depth;
};

class MissingVariablesException : public basics::Exception {
 public:
  MissingVariablesException(Variable const* variable, ExecutionNode const* node,
                            basics::SourceLocation location);

#ifdef ARANGODB_USE_GOOGLE_TESTS
  MissingVariablesException(Variable const* variable,
                            tests::aql::ExecutionNodeMock const* node,
                            basics::SourceLocation location);
#endif

  auto variable() const noexcept -> Variable const*;
  auto node() const noexcept -> ExecutionNode const*;

 private:
  Variable const* _variable{};
  ExecutionNode const* _node{};
};

template<typename T>
std::ostream& operator<<(std::ostream& os, RegisterPlanT<T> const& r);

}  // namespace arangodb::aql

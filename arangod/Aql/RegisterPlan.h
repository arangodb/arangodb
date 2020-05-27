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

template <typename T>
struct RegisterPlanWalkerT final : public WalkerWorker<T> {
  explicit RegisterPlanWalkerT(std::shared_ptr<RegisterPlanT<T>> plan)
      : plan(std::move(plan)) {}
  virtual ~RegisterPlanWalkerT() noexcept = default;

  void after(T* eb) final;
  bool enterSubquery(T*, T*) final {
    return false;  // do not walk into subquery
  }

  using RegCountStack = std::stack<RegisterCount>;

  RegIdOrderedSetStack unusedRegisters{{}};
  RegIdSetStack regsToKeepStack{{}};
  std::shared_ptr<RegisterPlanT<T>> plan;
  RegCountStack previousSubqueryNrRegs{};
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

  // We collect the subquery nodes to deal with them at the end:
  std::vector<T*> subQueryNodes;

  /// @brief maximum register id that can be assigned, plus one.
  /// this is used for assertions
  static constexpr RegisterId MaxRegisterId = RegisterId{1000};

 public:
  RegisterPlanT();
  RegisterPlanT(arangodb::velocypack::Slice slice, unsigned int depth);
  // Copy constructor used for a subquery:
  RegisterPlanT(RegisterPlan const& v, unsigned int newdepth);
  ~RegisterPlanT() = default;

  std::shared_ptr<RegisterPlanT> clone();

  void registerVariable(Variable const* v, std::set<RegisterId>& unusedRegisters);
  void increaseDepth();
  auto addRegister() -> RegisterId;
  void addSubqueryNode(T* subquery);

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  static void toVelocyPackEmpty(arangodb::velocypack::Builder& builder);

  auto variableToRegisterId(Variable const* variable) const -> RegisterId;

  // compatibility function for 3.6. can be removed in 3.8
  auto calcRegsToKeep(VarSetStack const& varsUsedLaterStack, VarSetStack const& varsValidStack,
                      std::vector<Variable const*> const& varsSetHere) const -> RegIdSetStack;

 private:
  unsigned int depth;
};

template <typename T>
std::ostream& operator<<(std::ostream& os, RegisterPlanT<T> const& r);

}  // namespace arangodb::aql

#endif

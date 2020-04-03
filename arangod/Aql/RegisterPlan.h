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
#include <unordered_map>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
}

namespace arangodb::aql {

class ExecutionNode;
class ExecutionPlan;
struct Variable;

/// @brief static analysis, walker class and information collector
struct VarInfo {
  unsigned int depth;
  RegisterId registerId;

  VarInfo() = delete;
  VarInfo(int depth, RegisterId registerId);
};

struct RegisterPlan final : public WalkerWorker<ExecutionNode> {
  // The following are collected for global usage in the ExecutionBlock,
  // although they are stored here in the node:

  // map VariableIds to their depth and registerId:
  std::unordered_map<VariableId, VarInfo> varInfo;

  // number of variables in this and all outer frames together,
  // the entry with index i here is always the sum of all values
  // in nrRegsHere from index 0 to i (inclusively) and the two
  // have the same length:
  std::vector<RegisterId> nrRegs;

  // We collect the subquery nodes to deal with them at the end:
  std::vector<ExecutionNode*> subQueryNodes;

 private:
  // Local for the walk:
  unsigned int depth;
  
  unsigned int totalNrRegs;

  // This is used to tell all nodes and share a pointer to ourselves
  std::shared_ptr<RegisterPlan>* me;

 public:
  RegisterPlan();
  RegisterPlan(arangodb::velocypack::Slice slice, unsigned int depth);
  // Copy constructor used for a subquery:
  RegisterPlan(RegisterPlan const& v, unsigned int newdepth);
  ~RegisterPlan() = default;
  
  void setSharedPtr(std::shared_ptr<RegisterPlan>* shared) { me = shared; }

  std::shared_ptr<RegisterPlan> clone();

  void registerVariable(Variable const* v);
  
  void increaseDepth();
  
  void addRegister();

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  
  static void toVelocyPackEmpty(arangodb::velocypack::Builder& builder);

  virtual bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;  // do not walk into subquery
  }

  virtual void after(ExecutionNode* eb) override final;

 public:
  /// @brief maximum register id that can be assigned, plus one.
  /// this is used for assertions
  static constexpr RegisterId MaxRegisterId = 1000;
};

}  // namespace arangodb::aql

#endif

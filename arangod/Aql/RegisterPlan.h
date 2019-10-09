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

#include "Aql/WalkerWorker.h"
#include "Aql/types.h"
#include "Basics/Common.h"
#include "Basics/debugging.h"

#include <memory>

namespace arangodb {
namespace aql {

class ExecutionNode;
class ExecutionPlan;

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

  // number of variables in the frame of the current depth:
  std::vector<RegisterId> nrRegsHere;

  // number of variables in this and all outer frames together,
  // the entry with index i here is always the sum of all values
  // in nrRegsHere from index 0 to i (inclusively) and the two
  // have the same length:
  std::vector<RegisterId> nrRegs;

  // We collect the subquery nodes to deal with them at the end:
  std::vector<ExecutionNode*> subQueryNodes;

  // Local for the walk:
  unsigned int depth;
  unsigned int totalNrRegs;

 private:
  // This is used to tell all nodes and share a pointer to ourselves
  std::shared_ptr<RegisterPlan>* me;

 public:
  RegisterPlan() : depth(0), totalNrRegs(0), me(nullptr) {
    nrRegsHere.reserve(8);
    nrRegsHere.emplace_back(0);
    nrRegs.reserve(8);
    nrRegs.emplace_back(0);
  }

  void clear();

  void setSharedPtr(std::shared_ptr<RegisterPlan>* shared) { me = shared; }

  // Copy constructor used for a subquery:
  RegisterPlan(RegisterPlan const& v, unsigned int newdepth);
  ~RegisterPlan() = default;

  virtual bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return false;  // do not walk into subquery
  }

  virtual void after(ExecutionNode* eb) override final;

  RegisterPlan* clone(ExecutionPlan* otherPlan, ExecutionPlan* plan);

 public:
  /// @brief maximum register id that can be assigned, plus one.
  /// this is used for assertions
  static constexpr RegisterId MaxRegisterId = 1000;

  /// @brief reserved register id for subquery depth. Needs to
  ///        be present in all AqlItemMatrixes.
  static constexpr RegisterId SUBQUERY_DEPTH_REGISTER = 0;
};

}  // namespace aql
}  // namespace arangodb

#endif

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_EXECUTION_PLAN_VERIFY_CLUSTER_H
#define ARANGOD_AQL_EXECUTION_PLAN_VERIFY_CLUSTER_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"

// This walker verifies that the execution location of planned nodes is consistent:
//
//  - Nodes that can only be run on a DBServer are asserted to be planned to run on a DBServer
//  - Nodes that can only be run on a Coordinator are asserted to be planned to run on a Coordinator
//
// any inconsistency can then be asserted/thrown as an exception
//
// TODO: This kind of defines which nodes we *expect* to be run on a DBServer/Coordinator
//       so should be part of the Optimizer Rule Reworking Project
//
// Should a node have a method `executionLocation` that returns one of COORDINATOR or DBSERVER?
//
namespace arangodb {
namespace aql {

class VerifyClusterExecution : public WalkerWorker<ExecutionNode> {
 public:
  VerifyClusterExecution(ExecutionPlan& plan);
  bool before(ExecutionNode* node) override;

private:
  enum class ExecutionLocation { COORDINATOR, DBSERVER };

 private:
  ExecutionPlan& _plan;
  ExecutionLocation _where{ExecutionLocation::COORDINATOR};
};

}  // namespace aql
}  // namespace arangodb
#endif

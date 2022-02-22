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

#include "Aql/ExecutionPlan/VerifyClusterExecution.h"

#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

using namespace arangodb::basics;
using namespace arangodb::aql;

namespace arangodb {
namespace aql {

VerifyClusterExecution::VerifyClusterExecution(ExecutionPlan& plan)
    : _plan{plan}, _where{ExecutionLocation::COORDINATOR} {}

// TODO: Why does LOG << node->id() not work?
auto VerifyClusterExecution::before(ExecutionNode* node) -> bool {
  bool ok = true;
  std::vector<std::stringstream> errors;

  switch (node->getType()) {
    case ExecutionNode::REMOTE: {
      switch (_where) {
        case ExecutionLocation::COORDINATOR: {
          _where = ExecutionLocation::DBSERVER;
        } break;
        case ExecutionLocation::DBSERVER: {
          _where = ExecutionLocation::COORDINATOR;
        } break;
      };
    } break;

  // definitely on DBServer
  case ExecutionNode::SORT:
  case ExecutionNode::UPSERT:
  case ExecutionNode::REMOVE:
  case ExecutionNode::INSERT:
  case ExecutionNode::REPLACE:
  case ExecutionNode::UPDATE:
  case ExecutionNode::REMOTESINGLE:
  case ExecutionNode::ENUMERATE_COLLECTION:
  case ExecutionNode::ENUMERATE_LIST:
  case ExecutionNode::INDEX:
  case ExecutionNode::MATERIALIZE:
  case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
    if (_where == ExecutionLocation::COORDINATOR) {
      errors.emplace_back() << "execution node [" << node->id().id() << "] "
                            << node->getTypeString()
                            << " is scheduled to run on Coordinator, but must run on DBServer";
      ok = false;
    }
  } break;

  // definitely on coordinator
  case ExecutionNode::SINGLETON:
  case ExecutionNode::LIMIT:
  case ExecutionNode::SCATTER:
  case ExecutionNode::GATHER:
  case ExecutionNode::DISTRIBUTE: {
    if (_where == ExecutionLocation::DBSERVER) {
      errors.emplace_back() << "execution node [" << node->id().id() << "] "
                            << node->getTypeString()
                            << " is scheduled to run on DBServer, but must run on Coordinator";
      ok = false;
    }
  } break;

  // good guys and bad guys on either side
  // here we need to establish where a node can run
  case ExecutionNode::FILTER: {
    // a filter node can both run on dbserver, if the filter expression is available on dbserver
  } break;

  case ExecutionNode::CALCULATION: {
    // a calculation can run on DBServer when its contained expression can.
  }
  case ExecutionNode::COLLECT: {
    // no idea
  }
  case ExecutionNode::RETURN:

  case ExecutionNode::TRAVERSAL:
  case ExecutionNode::SHORTEST_PATH:
  case ExecutionNode::K_SHORTEST_PATHS: {
    // the traversals can be pushed to a dbserver on disjoint samrt graphs, and for satellite graph
    // traversals and joins.
    // At this point the node itself should know what the what is.
    //
    // Actually the node should be able to tell us, and this would make optimizer rules
    // less awkward.
  } break;

  case ExecutionNode::DISTRIBUTE_CONSUMER:
  case ExecutionNode::SUBQUERY:

  case ExecutionNode::SUBQUERY_START:
  case ExecutionNode::SUBQUERY_END: {
    // these can be pushed to a DBServer on disjoint smart graps
  } break;

  // don't know.
  case  ExecutionNode::ASYNC:
  case  ExecutionNode::MUTEX:

  default:
    TRI_ASSERT(false);
    break;
  }

  if (!ok) {
    LOG_TOPIC("d45f8", ERR, arangodb::Logger::AQL) << "Kaput plan:";
    _plan.show();
    LOG_TOPIC("c14a2", ERR, arangodb::Logger::AQL)
        << "encountered the following error(s):";
    for (auto const& err : errors) {
      LOG_TOPIC("17a18", ERR, arangodb::Logger::AQL) << err.str();
    }
  }
  TRI_ASSERT(ok);

  return false;
}

}  // namespace aql
}  // namespace arangodb

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_NODE_COUNTER_H
#define ARANGOD_AQL_NODE_COUNTER_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"
#include "Cluster/ServerState.h"

#include <array>
#include <unordered_set>

namespace arangodb {
namespace aql {

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
/// @brief validate the counters of the plan
struct NodeCounter final : public WalkerWorker<ExecutionNode> {
  std::array<uint32_t, ExecutionNode::MAX_NODE_TYPE_VALUE> counts;
  std::unordered_set<ExecutionNode const*> seen;

  NodeCounter() : counts{} {}

  bool enterSubquery(ExecutionNode*, ExecutionNode*) override final {
    return true;
  }

  void after(ExecutionNode* en) override final {
    if (seen.find(en) == seen.end()) {
      // There is a chance that we have the same node twice
      // if we have multiple streams leading to it (e.g. Distribute)
      counts[en->getType()]++;
      seen.emplace(en);
    }
  }

  bool done(ExecutionNode* en) override final {
    if (en->getType() == ExecutionNode::MUTEX &&
        seen.find(en) != seen.end()) {
      return true;
    }
    if (!arangodb::ServerState::instance()->isDBServer() ||
        (en->getType() != ExecutionNode::REMOTE && en->getType() != ExecutionNode::SCATTER &&
         en->getType() != ExecutionNode::DISTRIBUTE)) {
      return WalkerWorker<ExecutionNode>::done(en);
    }
    return false;
  }
};

// template instantiation
template <>
bool ExecutionNode::walk(NodeCounter& worker);
#endif

} // namespace aql
} // namespace arangodb

#endif

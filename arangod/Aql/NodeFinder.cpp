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

#include "Aql/NodeFinder.h"

namespace arangodb {
namespace aql {

/// @brief node finder for one node type
template <>
NodeFinder<ExecutionNode::NodeType>::NodeFinder(
    ExecutionNode::NodeType lookingFor, SmallVector<ExecutionNode*>& out,
    bool enterSubqueries)
    : _lookingFor(lookingFor), _out(out), _enterSubqueries(enterSubqueries) {}

/// @brief node finder for multiple types
template <>
NodeFinder<std::vector<ExecutionNode::NodeType>>::NodeFinder(
    std::vector<ExecutionNode::NodeType> lookingFor,
    SmallVector<ExecutionNode*>& out, bool enterSubqueries)
    : _lookingFor(lookingFor), _out(out), _enterSubqueries(enterSubqueries) {}

/// @brief before method for one node type
template <>
bool NodeFinder<ExecutionNode::NodeType>::before(ExecutionNode* en) {
  if (en->getType() == _lookingFor) {
    _out.emplace_back(en);
  }

  return false;
}

/// @brief before method for multiple node types
template <>
bool NodeFinder<std::vector<ExecutionNode::NodeType>>::before(
    ExecutionNode* en) {
  auto const nodeType = en->getType();

  for (auto& type : _lookingFor) {
    if (type == nodeType) {
      _out.emplace_back(en);
      break;
    }
  }
  return false;
}

/// @brief node finder for one node type
EndNodeFinder::EndNodeFinder(SmallVector<ExecutionNode*>& out,
                             bool enterSubqueries)
    : _out(out), _found({false}), _enterSubqueries(enterSubqueries) {}

/// @brief before method for one node type
bool EndNodeFinder::before(ExecutionNode* en) {
  TRI_ASSERT(!_found.empty());
  if (!_found.back()) {
    // no node found yet. note that we found one on this level
    _out.emplace_back(en);
    _found[_found.size() - 1] = true;
  }

  // if we don't need to enter subqueries, we can stop after the first node that
  // we found
  return (!_enterSubqueries);
}

}  // namespace arangodb::aql
}  // namespace arangodb

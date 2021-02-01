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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "MockTypedNode.h"

#include "Aql/ExecutionNodeId.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

MockTypedNode::MockTypedNode(ExecutionPlan* plan, ExecutionNodeId id, NodeType type)
    : ExecutionNode(plan, id), _mockedType(type) {}

ExecutionNode* MockTypedNode::clone(ExecutionPlan* plan, bool withDependencies,
                                    bool withProperties) const {
  return cloneHelper(std::make_unique<MockTypedNode>(plan, _id, _mockedType),
                     withDependencies, withProperties);
}

::arangodb::aql::CostEstimate MockTypedNode::estimateCost() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<::arangodb::aql::ExecutionBlock> MockTypedNode::createBlock(
    ::arangodb::aql::ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ::arangodb::aql::ExecutionBlock*> const&) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void MockTypedNode::toVelocyPackHelper(arangodb::velocypack::Builder&, unsigned flags,
                                       std::unordered_set<ExecutionNode const*>& seen) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

ExecutionNode::NodeType MockTypedNode::getType() const { return _mockedType; }

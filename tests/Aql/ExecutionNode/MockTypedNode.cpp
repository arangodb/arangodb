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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "MockTypedNode.h"

#include "Aql/ExecutionNodeId.h"
#include "Basics/Exceptions.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

MockTypedNode::MockTypedNode(ExecutionPlan* plan, ExecutionNodeId id,
                             NodeType type)
    : ExecutionNode(plan, id), _mockedType(type) {}

ExecutionNode* MockTypedNode::clone(ExecutionPlan* plan,
                                    bool withDependencies) const {
  return cloneHelper(std::make_unique<MockTypedNode>(plan, _id, _mockedType),
                     withDependencies);
}

::arangodb::aql::CostEstimate MockTypedNode::estimateCost() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<::arangodb::aql::ExecutionBlock> MockTypedNode::createBlock(
    ::arangodb::aql::ExecutionEngine& engine) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void MockTypedNode::doToVelocyPack(arangodb::velocypack::Builder&,
                                   unsigned flags) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

ExecutionNode::NodeType MockTypedNode::getType() const { return _mockedType; }

size_t MockTypedNode::getMemoryUsedBytes() const { return sizeof(*this); }

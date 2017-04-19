////////////////////////////////////////////////////////////////////////////////
/// @brief Generic Node for Graph operations in AQL
///
/// @file arangod/Aql/GraphNode.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "GraphNode.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Graph/BaseOptions.h"

using namespace arangodb::aql;
using namespace arangodb::graph;

GraphNode::GraphNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
                     std::unique_ptr<BaseOptions>& options)
    : ExecutionNode(plan, id),
      _vocbase(vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _graphObj(nullptr),
      _tmpObjVariable(_plan->getAst()->variables()->createTemporaryVariable()),
      _tmpObjVarNode(_plan->getAst()->createNodeReference(_tmpObjVariable)),
      _tmpIdNode(_plan->getAst()->createNodeValueString("", 0)),
      _options(std::move(options)),
      _optionsBuild(false),
      _isSmart(false) {
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_options != nullptr);
}

GraphNode::GraphNode(ExecutionPlan* plan,
                     arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _graphObj(nullptr),
      _tmpObjVariable(nullptr),
      _tmpObjVarNode(nullptr),
      _tmpIdNode(nullptr),
      _options(nullptr),
      _optionsBuild(false),
      _isSmart(false) {}

GraphNode::~GraphNode() {}

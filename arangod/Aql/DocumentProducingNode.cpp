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

#include "DocumentProducingNode.h"

#include "Aql/AstNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Variable.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

DocumentProducingNode::DocumentProducingNode(Variable const* outVariable)
    : _outVariable(outVariable) {
  TRI_ASSERT(_outVariable != nullptr);
}

DocumentProducingNode::DocumentProducingNode(ExecutionPlan* plan,
                                             arangodb::velocypack::Slice slice)
    : _outVariable(Variable::varFromVPack(plan->getAst(), slice, "outVariable")) {
  TRI_ASSERT(_outVariable != nullptr);

  if (slice.hasKey("projection")) {
    // old format
    VPackSlice p = slice.get("projection");
    if (p.isArray()) {
      for (VPackSlice it : VPackArrayIterator(p)) {
        _projections.emplace_back(it.copyString());
        break;  // stop after first sub-attribute!
      }
    }
  } else if (slice.hasKey("projections")) {
    // new format
    VPackSlice p = slice.get("projections");
    if (p.isArray()) {
      for (VPackSlice it : VPackArrayIterator(p)) {
        if (it.isString()) {
          _projections.emplace_back(it.copyString());
        }
      }
    }
  }

  if (slice.hasKey("filter")) {
    Ast* ast = plan->getAst();
    // new AstNode is memory-managed by the Ast
    setFilter(std::make_unique<Expression>(plan, ast, new AstNode(ast, slice.get("filter"))));
  }
}
  
void DocumentProducingNode::cloneInto(ExecutionPlan* plan, DocumentProducingNode& c) const {
  if (_filter != nullptr) {
    c.setFilter(std::unique_ptr<Expression>(_filter->clone(plan, plan->getAst())));
  }
}

void DocumentProducingNode::toVelocyPack(arangodb::velocypack::Builder& builder,
                                         unsigned flags) const {
  builder.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(builder);

  // export in new format
  builder.add("projections", VPackValue(VPackValueType::Array));
  for (auto const& it : _projections) {
    builder.add(VPackValue(it));
  }
  builder.close(); // projections
  
  if (_filter != nullptr) {
    builder.add(VPackValue("filter"));
    _filter->toVelocyPack(builder, flags);
  
    builder.add("producesResult", VPackValue(true));
  } else {
    builder.add("producesResult", VPackValue(dynamic_cast<ExecutionNode const*>(this)->isVarUsedLater(_outVariable)));
  }
}

Variable const* DocumentProducingNode::outVariable() const {
  return _outVariable;
}
  
/// @brief remember the condition to execute for early filtering
void DocumentProducingNode::setFilter(std::unique_ptr<Expression> filter) {
  _filter = std::move(filter);
}

std::vector<std::string> const& DocumentProducingNode::projections() const noexcept {
  return _projections;
}

void DocumentProducingNode::projections(std::vector<std::string> const& projections) {
  _projections = projections;
}

void DocumentProducingNode::projections(std::unordered_set<std::string>&& projections) {
  _projections.clear();
  _projections.reserve(projections.size());
  for (auto& it : projections) {
    _projections.push_back(std::move(it));
  }
}

void DocumentProducingNode::projections(std::vector<std::string>&& projections) noexcept {
  _projections = std::move(projections);
}

std::vector<size_t> const& DocumentProducingNode::coveringIndexAttributePositions() const noexcept {
  return _coveringIndexAttributePositions;
}

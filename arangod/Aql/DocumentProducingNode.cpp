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
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

namespace {
arangodb::velocypack::StringRef const filterKey("filter");
arangodb::velocypack::StringRef const producesResultKey("producesResult");
arangodb::velocypack::StringRef const projectionsKey("projections");
}

DocumentProducingNode::DocumentProducingNode(Variable const* outVariable)
    : _outVariable(outVariable),
      _count(false) {
  TRI_ASSERT(_outVariable != nullptr);
}

DocumentProducingNode::DocumentProducingNode(ExecutionPlan* plan,
                                             arangodb::velocypack::Slice slice)
    : _outVariable(Variable::varFromVPack(plan->getAst(), slice, "outVariable")),
      _count(false) {
  TRI_ASSERT(_outVariable != nullptr);

  VPackSlice p = slice.get(::projectionsKey);
  if (p.isArray()) {
    for (VPackSlice it : VPackArrayIterator(p)) {
      if (it.isString()) {
        _projections.emplace_back(it.copyString());
      }
    }
  }

  p = slice.get(::filterKey);
  if (!p.isNone()) {
    Ast* ast = plan->getAst();
    // new AstNode is memory-managed by the Ast
    setFilter(std::make_unique<Expression>(ast, new AstNode(ast, p)));
  }

  _count = arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "count", false);
}
  
void DocumentProducingNode::cloneInto(ExecutionPlan* plan, DocumentProducingNode& c) const {
  if (_filter != nullptr) {
    c.setFilter(std::unique_ptr<Expression>(_filter->clone(plan->getAst())));
  }
  c.copyCountFlag(this);
}

void DocumentProducingNode::toVelocyPack(arangodb::velocypack::Builder& builder,
                                         unsigned flags) const {
  builder.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(builder);

  builder.add(::projectionsKey, VPackValue(VPackValueType::Array));
  for (auto const& it : _projections) {
    builder.add(VPackValue(it));
  }
  builder.close(); // projections
  
  if (_filter != nullptr) {
    builder.add(VPackValuePair(::filterKey.data(), ::filterKey.size(), VPackValueType::String));
    _filter->toVelocyPack(builder, flags);
  }

  // "producesResult" is read by AQL explainer. don't remove it!
  builder.add("count", VPackValue(doCount()));
  if (doCount()) {
    TRI_ASSERT(_filter == nullptr);
    builder.add(::producesResultKey, VPackValue(false));
  } else {
    builder.add(::producesResultKey, VPackValue(_filter != nullptr || dynamic_cast<ExecutionNode const*>(this)->isVarUsedLater(_outVariable)));
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
  
bool DocumentProducingNode::doCount() const {
  return _count && (_filter == nullptr); 
}

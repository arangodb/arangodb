////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>

using namespace arangodb::aql;

DocumentProducingNode::DocumentProducingNode(Variable const* outVariable)
    : _outVariable(outVariable),
      _count(false),
      _useCache(true),
      _maxProjections(kMaxProjections) {
  TRI_ASSERT(_outVariable != nullptr);
}

DocumentProducingNode::DocumentProducingNode(ExecutionPlan* plan,
                                             arangodb::velocypack::Slice slice)
    : _outVariable(
          Variable::varFromVPack(plan->getAst(), slice, "outVariable")),
      _projections(arangodb::aql::Projections::fromVelocyPack(slice)),
      _filterProjections(arangodb::aql::Projections::fromVelocyPack(
          slice, "filterProjections")),
      _count(false),
      _useCache(true),
      _maxProjections(kMaxProjections) {
  TRI_ASSERT(_outVariable != nullptr);

  VPackSlice p = slice.get(StaticStrings::Filter);
  if (!p.isNone()) {
    Ast* ast = plan->getAst();
    // new AstNode is memory-managed by the Ast
    setFilter(std::make_unique<Expression>(ast, ast->createNode(p)));
  }

  _count = arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "count",
                                                               false);
  _readOwnWrites = arangodb::basics::VelocyPackHelper::getBooleanValue(
                       slice, StaticStrings::ReadOwnWrites, false)
                       ? ReadOwnWrites::yes
                       : ReadOwnWrites::no;

  _useCache = arangodb::basics::VelocyPackHelper::getBooleanValue(
      slice, StaticStrings::UseCache, _useCache);

  p = slice.get(StaticStrings::MaxProjections);
  if (!p.isNone()) {
    setMaxProjections(p.getNumber<size_t>());
  }
}

void DocumentProducingNode::cloneInto(ExecutionPlan* plan,
                                      DocumentProducingNode& c) const {
  if (_filter != nullptr) {
    c.setFilter(
        std::unique_ptr<Expression>(_filter->clone(plan->getAst(), true)));
  }
  c.copyCountFlag(this);
  c.setCanReadOwnWrites(canReadOwnWrites());
  c.setMaxProjections(maxProjections());
  c.setUseCache(useCache());
}

void DocumentProducingNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  if (hasFilter()) {
    _filter->replaceVariables(replacements);
  }
}

void DocumentProducingNode::toVelocyPack(arangodb::velocypack::Builder& builder,
                                         unsigned flags) const {
  builder.add(VPackValue("outVariable"));
  _outVariable->toVelocyPack(builder);

  _projections.toVelocyPack(builder);

  if (_filter != nullptr) {
    builder.add(VPackValue(StaticStrings::Filter));
    _filter->toVelocyPack(builder, flags);

    _filterProjections.toVelocyPack(builder, "filterProjections");
  } else {
    builder.add("filterProjections", VPackValue(VPackValueType::Array));
    builder.close();
  }

  // "producesResult" is read by AQL explainer. don't remove it!
  builder.add("count", VPackValue(doCount()));
  if (doCount()) {
    TRI_ASSERT(_filter == nullptr);
    builder.add(StaticStrings::ProducesResult, VPackValue(false));
  } else {
    builder.add(
        StaticStrings::ProducesResult,
        VPackValue(_filter != nullptr ||
                   dynamic_cast<ExecutionNode const*>(this)->isVarUsedLater(
                       _outVariable)));
  }
  builder.add(StaticStrings::ReadOwnWrites,
              VPackValue(_readOwnWrites == ReadOwnWrites::yes));

  builder.add(StaticStrings::UseCache, VPackValue(useCache()));
  builder.add(StaticStrings::MaxProjections, VPackValue(maxProjections()));
}

Variable const* DocumentProducingNode::outVariable() const {
  return _outVariable;
}

/// @brief remember the condition to execute for early filtering
void DocumentProducingNode::setFilter(std::unique_ptr<Expression> filter) {
  _filter = std::move(filter);
}

arangodb::aql::Projections const& DocumentProducingNode::projections()
    const noexcept {
  return _projections;
}

arangodb::aql::Projections& DocumentProducingNode::projections() noexcept {
  return _projections;
}

arangodb::aql::Projections const& DocumentProducingNode::filterProjections()
    const noexcept {
  return _filterProjections;
}

void DocumentProducingNode::setProjections(
    arangodb::aql::Projections projections) {
  _projections = std::move(projections);
}

bool DocumentProducingNode::doCount() const {
  return _count && (_filter == nullptr);
}

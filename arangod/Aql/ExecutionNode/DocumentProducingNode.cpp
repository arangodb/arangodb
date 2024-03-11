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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "DocumentProducingNode.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/QueryContext.h"
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
      _projections(aql::Projections::fromVelocyPack(
          plan->getAst(), slice, plan->getAst()->query().resourceMonitor())),
      _filterProjections(aql::Projections::fromVelocyPack(
          plan->getAst(), slice, "filterProjections",
          plan->getAst()->query().resourceMonitor())),
      _count(false),
      _useCache(true),
      _maxProjections(kMaxProjections) {
  TRI_ASSERT(_outVariable != nullptr);

  VPackSlice p = slice.get(StaticStrings::Filter);
  if (!p.isNone()) {
    Ast* ast = plan->getAst();
    // new AstNode is memory-managed by the Ast
    DocumentProducingNode::setFilter(
        std::make_unique<Expression>(ast, ast->createNode(p)));
  }

  _count = arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "count",
                                                               false);
  _readOwnWrites =
      ReadOwnWrites{arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, StaticStrings::ReadOwnWrites, false)};

  _useCache = arangodb::basics::VelocyPackHelper::getBooleanValue(
      slice, StaticStrings::UseCache, _useCache);

  p = slice.get(StaticStrings::MaxProjections);
  if (!p.isNone()) {
    setMaxProjections(p.getNumber<size_t>());
  }
}

void DocumentProducingNode::cloneInto(ExecutionPlan* plan,
                                      DocumentProducingNode& c) const {
  if (hasFilter()) {
    c.setFilter(_filter->clone(plan->getAst(), true));
  }
  c._projections = _projections;
  c._filterProjections = _filterProjections;
  c.copyCountFlag(this);
  c.setCanReadOwnWrites(canReadOwnWrites());
  c.setMaxProjections(maxProjections());
  c.setUseCache(useCache());
}

void DocumentProducingNode::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  if (hasFilter()) {
    filter()->replaceVariables(replacements);
  }
}

void DocumentProducingNode::replaceAttributeAccess(
    ExecutionNode const* self, Variable const* searchVariable,
    std::span<std::string_view> attribute, Variable const* replaceVariable) {
  if (hasFilter() && self != dynamic_cast<ExecutionNode const*>(this)) {
    filter()->replaceAttributeAccess(searchVariable, attribute,
                                     replaceVariable);
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

  builder.add("count", VPackValue(doCount()));
  // "producesResult" is read by AQL explainer. don't remove it!
  if (doCount()) {
    TRI_ASSERT(_filter == nullptr);
    builder.add(StaticStrings::ProducesResult, VPackValue(false));
  } else {
    builder.add(StaticStrings::ProducesResult, VPackValue(isProduceResult()));
  }
  builder.add(StaticStrings::ReadOwnWrites,
              VPackValue(_readOwnWrites == ReadOwnWrites::yes));

  builder.add(StaticStrings::UseCache, VPackValue(useCache()));
  builder.add(StaticStrings::MaxProjections, VPackValue(maxProjections()));
}

Variable const* DocumentProducingNode::outVariable() const {
  return _outVariable;
}

std::vector<Variable const*> DocumentProducingNode::getVariablesSetHere()
    const {
  std::vector<Variable const*> result;
  if (_projections.empty()) {
    // no projections. simply produce outvariable
    result.push_back(_outVariable);
  } else {
    // projections. add one output register per projection
    result.reserve(_projections.size() + 1);
    result.push_back(_outVariable);
    for (size_t i = 0; i < _projections.size(); ++i) {
      // output registers are not necessarily set yet
      if (_projections[i].variable != nullptr) {
        result.push_back(_projections[i].variable);
      }
    }
  }
  return result;
}

/// @brief remember the condition to execute for early filtering
void DocumentProducingNode::setFilter(std::unique_ptr<Expression> filter) {
  _filter = std::move(filter);
}

Projections const& DocumentProducingNode::projections() const noexcept {
  return _projections;
}

Projections& DocumentProducingNode::projections() noexcept {
  return _projections;
}

Projections const& DocumentProducingNode::filterProjections() const noexcept {
  return _filterProjections;
}

void DocumentProducingNode::setProjections(aql::Projections projections) {
  _projections = std::move(projections);
}

void DocumentProducingNode::setFilterProjections(aql::Projections projections) {
  _filterProjections = std::move(projections);
}

bool DocumentProducingNode::doCount() const noexcept {
  return _count && !hasFilter();
}

AsyncPrefetchEligibility DocumentProducingNode::canUseAsyncPrefetching()
    const noexcept {
  // we cannot use prefetching if the filter employs V8, because the
  // Query object only has a single V8 context, which it can enter and exit.
  // with prefetching, multiple threads can execute calculations in the same
  // Query instance concurrently, and when using V8, they could try to
  // enter/exit the V8 context of the query concurrently. this is currently
  // not thread-safe, so we don't use prefetching.
  // the constraint for determinism is there because we could produce
  // different query results when prefetching is enabled, at least in
  // streaming queries.
  return (!hasFilter() || (_filter->isDeterministic() && !_filter->willUseV8()))
             ? AsyncPrefetchEligibility::kEnableForNode
             : AsyncPrefetchEligibility::kDisableForNode;
}

bool DocumentProducingNode::recalculateProjections(ExecutionPlan* plan) {
  auto filterProjectionsHash = _filterProjections.hash();
  auto projectionsHash = _projections.hash();
  TRI_ASSERT(!_projections.hasOutputRegisters()) << _projections;
  TRI_ASSERT(!_filterProjections.hasOutputRegisters()) << _filterProjections;
  _filterProjections.clear();
  _projections.clear();

  containers::FlatHashSet<AttributeNamePath> attributes;
  if (hasFilter()) {
    if (Ast::getReferencedAttributesRecursive(
            this->filter()->node(), this->outVariable(),
            /*expectedAttribute*/ "", attributes,
            plan->getAst()->query().resourceMonitor())) {
      _filterProjections = aql::Projections{std::move(attributes)};
    }
  }

  attributes.clear();
  if (utils::findProjections(dynamic_cast<ExecutionNode*>(this), outVariable(),
                             /*expectedAttribute*/ "",
                             /*excludeStartNodeFilterCondition*/ true,
                             attributes)) {
    if (attributes.size() <= maxProjections()) {
      _projections = Projections(std::move(attributes));
    }
  }

  TRI_ASSERT(!_filterProjections.hasOutputRegisters());

  return projectionsHash != _projections.hash() ||
         filterProjectionsHash != _filterProjections.hash();
}

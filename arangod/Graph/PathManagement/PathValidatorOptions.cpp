////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "PathValidatorOptions.h"

#include "Aql/Expression.h"
#include "Aql/QueryContext.h"

using namespace arangodb;
using namespace arangodb::graph;

PathValidatorOptions::PathValidatorOptions(
    aql::Variable const* tmpVar,
    arangodb::aql::FixedVarExpressionContext& expressionContext)
    : _tmpVar(tmpVar), _expressionCtx(expressionContext) {}

void PathValidatorOptions::setAllVerticesExpression(
    std::unique_ptr<aql::Expression> expression) {
  // All edge expression should not be set before
  TRI_ASSERT(_allVerticesExpression == nullptr);
  _allVerticesExpression = std::move(expression);
}

void PathValidatorOptions::setPruneEvaluator(std::shared_ptr<aql::PruneExpressionEvaluator>&& expression) {
  _pruneEvaluator = std::move(expression);
}

void PathValidatorOptions::setPostFilterEvaluator(std::shared_ptr<aql::PruneExpressionEvaluator>&& expression) {
  _postFilterEvaluator = std::move(expression);
}

std::shared_ptr<aql::PruneExpressionEvaluator>& PathValidatorOptions::getPruneEvaluator() {
  return _pruneEvaluator;
}

std::shared_ptr<aql::PruneExpressionEvaluator>& PathValidatorOptions::getPostFilterEvaluator() {
  return _postFilterEvaluator;
}

bool PathValidatorOptions::usesPrune() const {
  return _pruneEvaluator != nullptr;
}

bool PathValidatorOptions::usesPostFilter() const {
  return _postFilterEvaluator != nullptr;
}

void PathValidatorOptions::setPruneContext(arangodb::aql::InputAqlItemRow& inputRow) {
  TRI_ASSERT(_pruneEvaluator != nullptr);
  _pruneEvaluator->prepareContext(inputRow);
}

void PathValidatorOptions::setPostFilterContext(arangodb::aql::InputAqlItemRow& inputRow) {
  TRI_ASSERT(_postFilterEvaluator != nullptr);
  _postFilterEvaluator->prepareContext(inputRow);
}

void PathValidatorOptions::setVertexExpression(uint64_t depth,
                                               std::unique_ptr<aql::Expression> expression) {
  // Should not respecifiy the condition on a certain depth
  TRI_ASSERT(_vertexExpressionOnDepth.find(depth) ==
             _vertexExpressionOnDepth.end());
  _vertexExpressionOnDepth.emplace(depth, std::move(expression));
}

aql::Expression* PathValidatorOptions::getVertexExpression(
    uint64_t depth) const {
  auto const& it = _vertexExpressionOnDepth.find(depth);
  if (it != _vertexExpressionOnDepth.end()) {
    return it->second.get();
  }
  return _allVerticesExpression.get();
}

void PathValidatorOptions::unpreparePruneContext() {
  if (_pruneEvaluator != nullptr) {
    _pruneEvaluator->unPrepareContext();
  }
}

void PathValidatorOptions::unpreparePostFilterContext() {
  if (_postFilterEvaluator != nullptr) {
    _postFilterEvaluator->unPrepareContext();
  }
}

void PathValidatorOptions::addAllowedVertexCollection(std::string const& collectionName) {
  TRI_ASSERT(std::find(_allowedVertexCollections.begin(),
                       _allowedVertexCollections.end(),
                       collectionName) == _allowedVertexCollections.end());
  _allowedVertexCollections.emplace_back(collectionName);
}

void PathValidatorOptions::addAllowedVertexCollections(
    std::vector<std::string> const& collectionNames) {
  for (auto& name : collectionNames) {
    addAllowedVertexCollection(name);
  }
}

std::vector<std::string> const&
PathValidatorOptions::getAllowedVertexCollections() const {
  return _allowedVertexCollections;
}

aql::Variable const* PathValidatorOptions::getTempVar() const {
  return _tmpVar;
}

aql::FixedVarExpressionContext& PathValidatorOptions::getExpressionContext() {
  return _expressionCtx;
}

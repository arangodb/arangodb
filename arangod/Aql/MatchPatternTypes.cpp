////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "MatchPatternTypes.h"

#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"

namespace arangodb::aql {

MatchDataSource MatchDataSource::collection(std::string name) {
  return MatchDataSource{Kind::kCollection, std::move(name)};
}

MatchDataSource MatchDataSource::bindParameter(std::string name) {
  return MatchDataSource{Kind::kBindParameter, std::move(name)};
}

MatchDataSource::MatchDataSource(Kind kind, std::string name)
    : _kind{kind}, _name{std::move(name)} {}

MatchPathRange MatchPathRange::defaultFixedOne() {
  return MatchPathRange{Kind::kDefaultFixedOne, 1, 1};
}

MatchPathRange MatchPathRange::bounded(uint64_t minDepth, uint64_t maxDepth) {
  return MatchPathRange{Kind::kBounded, minDepth, maxDepth};
}

MatchPathRange MatchPathRange::unboundedMin(uint64_t minDepth) {
  return MatchPathRange{Kind::kUnboundedMin, minDepth, std::nullopt};
}

MatchPathRange::MatchPathRange(Kind kind, uint64_t minDepth,
                               std::optional<uint64_t> maxDepth)
    : _kind{kind}, _minDepth{minDepth}, _maxDepth{maxDepth} {}

bool MatchPathRange::isFixedOne() const noexcept {
  return _minDepth == 1 && _maxDepth == 1;
}

bool MatchPathRange::isFixed() const noexcept {
  return _maxDepth.has_value() && _minDepth == *_maxDepth;
}

MatchProjectionReservedAttribute classifyMatchProjectionReservedAttribute(
    std::string_view name, bool isEdge) noexcept {
  if (name == StaticStrings::IdString) {
    return MatchProjectionReservedAttribute::kId;
  }
  if (isEdge) {
    if (name == StaticStrings::FromString) {
      return MatchProjectionReservedAttribute::kFrom;
    }
    if (name == StaticStrings::ToString) {
      return MatchProjectionReservedAttribute::kTo;
    }
  }
  return MatchProjectionReservedAttribute::kNone;
}

bool isMatchProjectionReservedAttribute(std::string_view name,
                                        bool isEdge) noexcept {
  return classifyMatchProjectionReservedAttribute(name, isEdge) !=
         MatchProjectionReservedAttribute::kNone;
}

std::vector<std::string_view> mandatoryMatchProjectionAttributes(bool isEdge) {
  if (isEdge) {
    return {StaticStrings::IdString, StaticStrings::FromString,
            StaticStrings::ToString};
  }
  return {StaticStrings::IdString};
}

MatchProjectionItem MatchProjectionItem::keepPath(
    std::vector<std::string> path) {
  TRI_ASSERT(!path.empty());
  MatchProjectionItem item;
  item.kind = Kind::kKeepAttribute;
  item.name = path.size() == 1 ? path.front() : std::string{};
  item.path = std::move(path);
  return item;
}

MatchProjectionItem MatchProjectionItem::keepLiteral(std::string key) {
  MatchProjectionItem item;
  item.kind = Kind::kKeepLiteral;
  item.path = {key};
  item.name = std::move(key);
  return item;
}

MatchProjectionItem MatchProjectionItem::alias(std::string name,
                                               MatchExpressionRef expression) {
  MatchProjectionItem item;
  item.kind = Kind::kAlias;
  item.name = std::move(name);
  item.expression = expression;
  return item;
}

std::string_view MatchProjectionItem::topLevelKey() const noexcept {
  if (isAlias()) {
    return name;
  }
  TRI_ASSERT(!path.empty());
  return path.front();
}

Variable const* MatchPatternElement::outputVariable() const noexcept {
  if (kind == Kind::kVariableReference) {
    return variableReference;
  }
  TRI_ASSERT(vertex.has_value());
  return vertex->variable;
}

}  // namespace arangodb::aql

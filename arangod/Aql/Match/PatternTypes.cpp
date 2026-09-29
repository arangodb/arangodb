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

#include "Aql/Match/PatternTypes.h"

#include "Aql/Variable.h"
#include "Basics/debugging.h"

namespace arangodb::aql::match {

DataSource DataSource::collection(std::string name) {
  return DataSource{Kind::kCollection, std::move(name)};
}

DataSource DataSource::bindParameter(std::string name) {
  return DataSource{Kind::kBindParameter, std::move(name)};
}

DataSource::DataSource(Kind kind, std::string name)
    : _kind{kind}, _name{std::move(name)} {}

PathRange PathRange::defaultFixedOne() {
  return PathRange{Kind::kDefaultFixedOne, 1, 1};
}

PathRange PathRange::bounded(uint64_t minDepth, uint64_t maxDepth) {
  return PathRange{Kind::kBounded, minDepth, maxDepth};
}

PathRange PathRange::unboundedMin(uint64_t minDepth) {
  return PathRange{Kind::kUnboundedMin, minDepth, std::nullopt};
}

PathRange::PathRange(Kind kind, uint64_t minDepth,
                     std::optional<uint64_t> maxDepth)
    : _kind{kind}, _minDepth{minDepth}, _maxDepth{maxDepth} {}

bool PathRange::isFixedOne() const noexcept {
  return _minDepth == 1 && _maxDepth == 1;
}

bool PathRange::isFixed() const noexcept {
  return _maxDepth.has_value() && _minDepth == *_maxDepth;
}

ProjectionItem ProjectionItem::keepPath(std::vector<std::string> path) {
  TRI_ASSERT(!path.empty());
  ProjectionItem item;
  item.kind = Kind::kKeepAttribute;
  item.name = path.size() == 1 ? path.front() : std::string{};
  item.path = std::move(path);
  return item;
}

ProjectionItem ProjectionItem::keepLiteral(std::string key) {
  ProjectionItem item;
  item.kind = Kind::kKeepLiteral;
  item.path = {key};
  item.name = std::move(key);
  return item;
}

ProjectionItem ProjectionItem::alias(std::string name,
                                     ExpressionRef expression) {
  ProjectionItem item;
  item.kind = Kind::kAlias;
  item.name = std::move(name);
  item.expression = expression;
  return item;
}

std::string_view ProjectionItem::topLevelKey() const noexcept {
  if (isAlias()) {
    return name;
  }
  TRI_ASSERT(!path.empty());
  return path.front();
}

}  // namespace arangodb::aql::match

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

Variable const* MatchPatternElement::outputVariable() const noexcept {
  if (kind == Kind::kVariableReference) {
    return variableReference;
  }
  TRI_ASSERT(vertex.has_value());
  return vertex->variable;
}

}  // namespace arangodb::aql

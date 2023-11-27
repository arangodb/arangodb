////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <Aql/Optimizer/ExpressionMatcher/Matchable.h>

namespace arangodb::aql::expression_matcher {

// Applies the matcher and if it succeeds registers the AqlNode* that
// it succeeded on in the MatchResult under the name `name`
template<Matchable M>
struct MatchWithName {
  auto apply(AstNode const* node) -> MatchResult {
    auto result = matcher.apply(node);

    if (result.isError()) {
      return result;
    }

    return MatchResult::match(name, node);
  }
  std::string name;
  M matcher;
};

// Helper for convenient template argument deduction
template<Matchable M>
auto matchWithName(std::string name, M matcher) -> MatchWithName<M> {
  return MatchWithName<M>{.name = std::move(name), .matcher = matcher};
};

}  // namespace arangodb::aql::expression_matcher

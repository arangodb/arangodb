////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_ORDER_FACTORY_H
#define ARANGOD_IRESEARCH__IRESEARCH_ORDER_FACTORY_H 1

#include "AqlHelper.h"

#include "VocBase/voc-types.h"

#include "search/sort.hpp"

namespace arangodb {
namespace aql {

class Ast;
struct AstNode;
class CalculationNode;
class Expression;
struct Variable;

}  // namespace aql

namespace iresearch {

struct QueryContext;

}  // namespace iresearch

namespace transaction {

class Methods;  // forward declaration

}  // namespace transaction

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @struct OrderFactory
////////////////////////////////////////////////////////////////////////////////
struct OrderFactory {
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief determine if the 'node' can be converted into an iresearch scorer
  ///        if 'scorer' != nullptr then also append build iresearch scorer there
  ////////////////////////////////////////////////////////////////////////////////
  static bool scorer(irs::sort::ptr* scorer, aql::AstNode const& node,
                     iresearch::QueryContext const& ctx);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief determine if the 'node' can be converted into an iresearch scorer
  ///        if 'scorer' != nullptr then also append build iresearch comparer there
  ////////////////////////////////////////////////////////////////////////////////
  static bool comparer(irs::sort::ptr* scorer, aql::AstNode const& node);

  OrderFactory() = delete;
};  // OrderFactory

////////////////////////////////////////////////////////////////////////////////
/// @struct Scorer
/// @brief represents IResearch scorer in AQL terms
////////////////////////////////////////////////////////////////////////////////
struct Scorer {
  Scorer() = default;

  constexpr Scorer(aql::Variable const* var, aql::AstNode const* node) noexcept
      : var(var), node(node) {}

  constexpr bool operator==(Scorer const& rhs) const noexcept {
    return var == rhs.var && node == rhs.node;
  }

  constexpr bool operator!=(Scorer const& rhs) const noexcept {
    return !(*this == rhs);
  }

  aql::Variable const* var{};  // scorer variable
  aql::AstNode const* node{};  // scorer node
};                             // Scorer

////////////////////////////////////////////////////////////////////////////////
/// @class ScorerReplacer
/// @brief utility class that replaces scorer function call with corresponding
///        reference access
////////////////////////////////////////////////////////////////////////////////
class ScorerReplacer {
 public:
  ScorerReplacer() = default;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief replaces all occurences of IResearch scorers in a specified node with
  ///        corresponding reference access
  ////////////////////////////////////////////////////////////////////////////////
  void replace(aql::CalculationNode& node);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief extracts replacement results for a given variable
  ////////////////////////////////////////////////////////////////////////////////
  void extract(aql::Variable const& var, std::vector<Scorer>& scorers);

  ////////////////////////////////////////////////////////////////////////////////
  /// @returns true if no scorers were replaced
  ////////////////////////////////////////////////////////////////////////////////
  bool empty() const noexcept { return _dedup.empty(); }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief visits all replaced scorer entries
  ////////////////////////////////////////////////////////////////////////////////
  template <typename Visitor>
  bool visit(Visitor visitor) const {
    for (auto& entry : _dedup) {
      if (!visitor(entry.first)) {
        return false;
      }
    }
    return true;
  }

 private:
  struct HashedScorer : Scorer {
    HashedScorer(aql::Variable const* var, aql::AstNode const* node)
        : Scorer(var, node), hash(iresearch::hash(node)) {}

    size_t hash;
  };  // HashedScorer

  struct ScorerHash {
    size_t operator()(HashedScorer const& key) const noexcept {
      return key.hash;
    }
  };  // ScorerHash

  struct ScorerEqualTo {
    bool operator()(HashedScorer const& lhs, HashedScorer const& rhs) const {
      return iresearch::equalTo(lhs.node, rhs.node);
    }
  };  // ScorerEqualTo

  typedef std::unordered_map<HashedScorer, aql::Variable const*, ScorerHash, ScorerEqualTo> DedupScorers;

  DedupScorers _dedup;
};  // ScorerReplacer

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_ORDER_FACTORY_H

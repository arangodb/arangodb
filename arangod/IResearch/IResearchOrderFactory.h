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

NS_BEGIN(arangodb)
NS_BEGIN(aql)

class Ast;
struct AstNode;
struct Expression;
struct Variable;

NS_END // aql

NS_BEGIN(iresearch)

struct QueryContext;

NS_END // iresearch

NS_BEGIN(transaction)

class Methods; // forward declaration

NS_END // transaction

NS_BEGIN(iresearch)

struct OrderFactory {
  struct Scorer {
    Scorer() = default;

    constexpr Scorer(
        aql::Variable const* var,
        aql::AstNode const* node
    ) noexcept
      : var(var), node(node) {
    }

    constexpr bool operator==(Scorer const& rhs) const noexcept {
      return var == rhs.var && node == rhs.node;
    }

    constexpr bool operator!=(Scorer const& rhs) const noexcept {
      return !(*this == rhs);
    }

    aql::Variable const* var{}; // scorer variable
    aql::AstNode const* node{}; // scorer node
  }; // Scorer

  struct ScorerHash {
    size_t operator()(Scorer const& key) const noexcept {
      return iresearch::hash(key.node);
    }
  };

  struct ScorerEqualTo {
    bool operator()(Scorer const& lhs, Scorer const& rhs) const {
      return iresearch::equalTo(lhs.node, rhs.node);
    }
  };

  typedef std::map<aql::Variable const*, std::vector<Scorer>> VarToScorers; // key - loop variable
  typedef std::unordered_map<Scorer, aql::Variable const*, ScorerHash, ScorerEqualTo> DedupScorers;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief replaces scorer functions in a specified expression
  ////////////////////////////////////////////////////////////////////////////////
  static void replaceScorers(
    VarToScorers& vars,
    DedupScorers& scorers,
    aql::Expression& expr
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief determine if the 'node' can be converted into an iresearch scorer
  ///        if 'scorer' != nullptr then also append build iresearch scorer there
  ////////////////////////////////////////////////////////////////////////////////
  static bool scorer(
    irs::sort::ptr* scorer,
    aql::AstNode const& node,
    iresearch::QueryContext const& ctx
  );

  static bool comparer(
    irs::sort::ptr* scorer,
    aql::AstNode const& node
  );
}; // OrderFactory

NS_END // iresearch
NS_END // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_ORDER_FACTORY_H

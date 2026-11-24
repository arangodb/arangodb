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

#include "RocksDBEngine/InvertedIndex/Expr.h"

#include "Aql/AstNode.h"
#include "Basics/debugging.h"
#include "RocksDBEngine/InvertedIndex/Matcher.h"
#include "RocksDBEngine/InvertedIndex/Format.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>

#include <algorithm>

namespace arangodb::inverted_index {
namespace {

Expr constant(bool value) {
  return Expr{.type = value ? Expr::Type::True : Expr::Type::False};
}

bool isConstant(Expr const& expr, bool value) {
  return expr.type == (value ? Expr::Type::True : Expr::Type::False);
}

/// all documents except the excluded ones
bool isComplement(Expr const& expr) {
  return expr.type == Expr::Type::And && expr.children.empty() &&
         !expr.excluded.empty();
}

Expr normalizeTerm(aql::AstNode const& value) {
  // the inline storage of the buffer holds a scalar without a heap allocation
  velocypack::Buffer<std::uint8_t> buffer;
  velocypack::Builder searched(buffer);
  value.toVelocyPackValue(searched);
  auto term = normalizedTerm(searched.slice());
  if (!term.has_value()) {
    return constant(false);
  }
  return Expr{.type = Expr::Type::Term, .term = std::move(*term)};
}

std::vector<Expr> normalizeAll(std::vector<TermExpr> const& exprs) {
  std::vector<Expr> result;
  result.reserve(exprs.size());
  for (auto const& e : exprs) {
    result.push_back(normalize(e));
  }
  return result;
}

/// A complement among the required children of an And only adds exclusions.
/// A complement or NeedsScan among the excluded children would need the set
/// of all documents.
Expr foldAnd(Expr const& expr) {
  Expr result{.type = Expr::Type::And};
  for (auto const& child : expr.children) {
    Expr f = fold(child);
    if (isConstant(f, false) || f.type == Expr::Type::NeedsScan) {
      return f;
    }
    if (isComplement(f)) {
      std::ranges::move(f.excluded, std::back_inserter(result.excluded));
    } else if (!isConstant(f, true)) {
      result.children.push_back(std::move(f));
    }
  }
  for (auto const& child : expr.excluded) {
    Expr f = fold(child);
    if (isConstant(f, true)) {
      return constant(false);
    }
    if (isComplement(f) || f.type == Expr::Type::NeedsScan) {
      return Expr{.type = Expr::Type::NeedsScan};
    }
    if (!isConstant(f, false)) {
      result.excluded.push_back(std::move(f));
    }
  }
  if (result.children.empty() && result.excluded.empty()) {
    return constant(true);
  }
  if (result.children.size() == 1 && result.excluded.empty()) {
    return std::move(result.children.front());
  }
  return result;
}

/// A complement in an Or would need the set of all documents.
Expr foldOr(Expr const& expr) {
  Expr result{.type = Expr::Type::Or};
  for (auto const& child : expr.children) {
    Expr f = fold(child);
    if (isConstant(f, true)) {
      return f;
    }
    if (isComplement(f) || f.type == Expr::Type::NeedsScan) {
      return Expr{.type = Expr::Type::NeedsScan};
    }
    if (!isConstant(f, false)) {
      result.children.push_back(std::move(f));
    }
  }
  if (result.children.empty()) {
    return constant(false);
  }
  if (result.children.size() == 1) {
    return std::move(result.children.front());
  }
  return result;
}

}  // namespace

Expr normalize(TermExpr const& expr) {
  switch (expr.type) {
    case TermExpr::Type::Term:
      return normalizeTerm(*expr.value);
    case TermExpr::Type::True:
      return constant(true);
    case TermExpr::Type::False:
      return constant(false);
    case TermExpr::Type::And:
    case TermExpr::Type::Or:
      return Expr{.type = expr.type == TermExpr::Type::And ? Expr::Type::And
                                                           : Expr::Type::Or,
                  .children = normalizeAll(expr.children),
                  .excluded = normalizeAll(expr.excluded)};
  }
  TRI_ASSERT(false);
  return constant(false);
}

Expr fold(Expr const& expr) {
  switch (expr.type) {
    case Expr::Type::Term:
    case Expr::Type::True:
    case Expr::Type::False:
    case Expr::Type::NeedsScan:
      return expr;
    case Expr::Type::And:
      return foldAnd(expr);
    case Expr::Type::Or:
      return foldOr(expr);
  }
  TRI_ASSERT(false);
  return expr;
}

bool needsScan(Expr const& folded) {
  return isConstant(folded, true) || isComplement(folded) ||
         folded.type == Expr::Type::NeedsScan;
}

bool matches(Expr const& expr, std::span<std::string const> terms) {
  auto matchesTerms = [&](Expr const& e) { return matches(e, terms); };
  switch (expr.type) {
    case Expr::Type::Term:
      return std::ranges::binary_search(terms, expr.term);
    case Expr::Type::True:
      return true;
    case Expr::Type::False:
    case Expr::Type::NeedsScan:
      return false;
    case Expr::Type::And:
      return std::ranges::all_of(expr.children, matchesTerms) &&
             std::ranges::none_of(expr.excluded, matchesTerms);
    case Expr::Type::Or:
      return std::ranges::any_of(expr.children, matchesTerms);
  }
  return false;
}

}  // namespace arangodb::inverted_index

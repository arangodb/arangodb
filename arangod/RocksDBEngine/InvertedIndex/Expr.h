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

#pragma once

#include <span>
#include <string>
#include <vector>

namespace arangodb::inverted_index {

struct TermExpr;

/// A TermExpr whose searched values are in stored form, see normalizedTerm.
struct Expr {
  /// NeedsScan: the condition matches documents that are in no bitmap, so
  /// only a scan over all documents can answer it
  enum class Type { Term, And, Or, True, False, NeedsScan };

  Type type = Type::Term;
  std::string term;
  /// And: all must match; Or: one must match
  std::vector<Expr> children;
  /// And only: documents matching any of these are removed
  std::vector<Expr> excluded;

  bool operator==(Expr const&) const = default;
};

/// A searched value without a stored form is in no document, so its term
/// becomes False.
Expr normalize(TermExpr const& expr);

/// Folds constants. The result is True, False, NeedsScan, or an expression
/// that only matches documents with at least one searched term, which the
/// bitmaps answer alone.
Expr fold(Expr const& expr);

/// whether a folded expression can only be answered by a scan over all
/// documents
bool needsScan(Expr const& folded);

/// whether a document with the given terms (sorted, distinct) matches
bool matches(Expr const& expr, std::span<std::string const> terms);

}  // namespace arangodb::inverted_index

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
/// @author Simran Spiller
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Import/DocumentTransformer.h"
#include "Basics/Result.h"

#include <memory>
#include <string>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
class Ast;
struct AstNode;
class Expression;
class QueryContext;
struct Variable;
}  // namespace aql

namespace transaction {
class Methods;
}

namespace import {

class ImportExpressionContext;

/// @brief Client-side AQL document transformer for arangoimport.
///
/// Phase 1: expression-only path (no subqueries).
/// Parses and validates the AQL transform query once at construction time,
/// then executes LET/FILTER/RETURN expressions per document.
///
/// Follows the ComputedValues pattern from arangod/VocBase/ComputedValues.cpp
/// and StandaloneCalculation from arangod/Aql/StandaloneCalculation.cpp.
class AqlDocumentTransformer final : public IDocumentTransformer {
 public:
  /// @brief Construct the transformer. Parses, validates, and compiles the
  /// AQL query string. Throws on parse or validation errors.
  /// @param queryString The AQL transform expression (LET/FILTER/RETURN)
  /// @param userBindVars Additional bind variables (JSON object, may be empty)
  /// @param vocbase A vocbase for the standalone query context
  AqlDocumentTransformer(std::string const& queryString,
                         velocypack::Slice userBindVars,
                         TRI_vocbase_t& vocbase);

  ~AqlDocumentTransformer() override;

  /// @brief Transform a single document.
  /// @param doc The input document as a VPack object
  /// @return TransformResult with action (emit/skip/error) and result data
  TransformResult transform(velocypack::Slice doc) override;

 private:
  /// @brief Validate the AST: only LET/FILTER/RETURN at top level,
  /// reject subqueries, validate function restrictions.
  void validateAndDecompose(aql::AstNode* root);

  /// @brief Walk expression subtrees to reject subqueries and forbidden
  /// functions. Called by validateAndDecompose.
  void validateExpressionTree(aql::AstNode const* node);

  /// @brief Replace @doc (and user bind params) with temporary variables
  void replaceBindParameters();

  /// @brief Compile Expression objects from AST subtrees
  void compileExpressions();

  // The standalone query context (provides Parser, Ast, stubs for
  // transaction/collection access — same pattern as ComputedValues).
  std::unique_ptr<aql::QueryContext> _queryContext;

  // The expression context for per-document evaluation.
  std::unique_ptr<ImportExpressionContext> _expressionContext;

  // Transaction from the query context (no-op, for interface satisfaction).
  transaction::Methods* _trx = nullptr;

  // Temporary variable that replaces @doc bind parameter in the AST.
  aql::Variable const* _docVariable = nullptr;

  // User-supplied additional bind variables (constant across all docs).
  // Maps variable name -> {Variable*, VPackBuilder holding the value}.
  struct UserBindVar {
    aql::Variable const* variable;
    velocypack::Builder value;
  };
  std::vector<UserBindVar> _userBindVars;

  // Decomposed top-level statements from the AST:
  struct LetBinding {
    aql::Variable const* variable;
    aql::AstNode* expressionNode;
    std::unique_ptr<aql::Expression> expression;
  };
  std::vector<LetBinding> _letBindings;

  struct FilterExpr {
    aql::AstNode* expressionNode;
    std::unique_ptr<aql::Expression> expression;
  };
  std::vector<FilterExpr> _filterExpressions;

  aql::AstNode* _returnExpressionNode = nullptr;
  std::unique_ptr<aql::Expression> _returnExpression;

  // Per-document scratch space for materialized LET values.
  // We need to keep AqlValues alive while subsequent expressions
  // reference them via setVariable (which stores a Slice pointing
  // into the AqlValue's buffer).
  struct MaterializedValue {
    aql::Variable const* variable;
    velocypack::Builder builder;  // holds the materialized VPack
  };
  std::vector<MaterializedValue> _materializedValues;
};

}  // namespace import
}  // namespace arangodb

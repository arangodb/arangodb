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

#include "AqlDocumentTransformer.h"
#include "ImportExpressionContext.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AqlTransaction.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/LazyConditions.h"
#include "Aql/Parser.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryString.h"
#include "Aql/Scopes.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/SmartContext.h"
#include "Transaction/Status.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::import;

namespace {
constexpr std::string_view docParameterName = "doc";

/// @brief Dummy transaction state for client-tool standalone AQL evaluation.
/// Identical to CalculationTransactionState in StandaloneCalculation.cpp.
class ImportTransactionState final : public TransactionState {
 public:
  explicit ImportTransactionState(TRI_vocbase_t& vocbase)
      : TransactionState(
            vocbase, TransactionId(0), transaction::Options(),
            transaction::OperationOriginInternal{"arangoimport custom query"}) {
    updateStatus(transaction::Status::RUNNING);
  }

  ~ImportTransactionState() override {
    if (status() == transaction::Status::RUNNING) {
      updateStatus(transaction::Status::ABORTED);
    }
  }

  [[nodiscard]] bool ensureSnapshot() override { return false; }
  [[nodiscard]] futures::Future<Result> beginTransaction(
      transaction::Hints) override {
    return Result{};
  }
  [[nodiscard]] futures::Future<Result> commitTransaction(
      transaction::Methods*) override {
    applyBeforeCommitCallbacks();
    updateStatus(transaction::Status::COMMITTED);
    applyAfterCommitCallbacks();
    return Result{};
  }
  [[nodiscard]] Result abortTransaction(transaction::Methods*) override {
    updateStatus(transaction::Status::ABORTED);
    return {};
  }
  Result triggerIntermediateCommit() override {
    return Result{TRI_ERROR_INTERNAL};
  }
  [[nodiscard]] futures::Future<Result> performIntermediateCommitIfRequired(
      DataSourceId) override {
    return Result{};
  }
  [[nodiscard]] uint64_t numPrimitiveOperations() const noexcept override {
    return 0;
  }
  [[nodiscard]] bool hasFailedOperations() const noexcept override {
    return false;
  }
  [[nodiscard]] uint64_t numCommits() const noexcept override { return 0; }
  [[nodiscard]] uint64_t numIntermediateCommits() const noexcept override {
    return 0;
  }
  void addIntermediateCommits(uint64_t) override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  [[nodiscard]] TRI_voc_tick_t lastOperationTick() const noexcept override {
    return 0;
  }
  std::unique_ptr<TransactionCollection> createTransactionCollection(
      DataSourceId, AccessMode::Type) override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
};

/// @brief Dummy transaction context for client-tool standalone AQL.
struct ImportTransactionContext final : public transaction::SmartContext {
  explicit ImportTransactionContext(TRI_vocbase_t& vocbase)
      : SmartContext(
            vocbase, transaction::Context::makeTransactionId(), nullptr,
            transaction::OperationOriginInternal{"arangoimport custom query"}),
        _state(vocbase) {}

  std::shared_ptr<TransactionState> acquireState(transaction::Options const&,
                                                 bool&) override {
    return {std::shared_ptr<TransactionState>(), &_state};
  }
  void unregisterTransaction() noexcept override {}
  std::shared_ptr<Context> clone() const override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

 private:
  ImportTransactionState _state;
};

/// @brief Lightweight QueryContext for client-tool AQL expression evaluation.
/// Does not require DatabaseFeature or a real vocbase — creates its own
/// minimal vocbase sufficient for parsing and expression execution.
class ImportQueryContext final : public QueryContext {
 public:
  explicit ImportQueryContext(TRI_vocbase_t& vocbase)
      : QueryContext(
            vocbase,
            transaction::OperationOriginInternal{"arangoimport custom query"}),
        _resolver(vocbase),
        _trxContext(vocbase) {
    _ast = std::make_unique<Ast>(*this, NON_CONST_PARAMETERS);
    _trx = AqlTransaction::create(newTrxContext(), _collections,
                                  _queryOptions.transactionOptions,
                                  std::unordered_set<std::string>{});
    _trx->addHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL);
    _trx->addHint(transaction::Hints::Hint::SINGLE_OPERATION);
    auto res = _trx->begin();
    if (res.fail()) {
      throw basics::Exception(std::move(res));
    }
  }

  QueryOptions const& queryOptions() const override { return _queryOptions; }
  QueryOptions& queryOptions() noexcept override { return _queryOptions; }
  double getLockTimeout() const noexcept override {
    return _queryOptions.transactionOptions.lockTimeout;
  }
  void setLockTimeout(double timeout) noexcept override {
    _queryOptions.transactionOptions.lockTimeout = timeout;
  }
  CollectionNameResolver const& resolver() const override { return _resolver; }
  velocypack::Options const& vpackOptions() const override {
    return velocypack::Options::Defaults;
  }
  std::shared_ptr<transaction::Context> newTrxContext() const override {
    return std::shared_ptr<transaction::Context>(
        std::shared_ptr<transaction::Context>(), &_trxContext);
  }
  transaction::Methods& trxForOptimization() override { return *_trx; }
  bool killed() const override { return false; }
  void debugKillQuery() override {}
  bool isModificationQuery() const noexcept override { return false; }
  bool isAsyncQuery() const noexcept override { return false; }

 private:
  QueryOptions _queryOptions;
  CollectionNameResolver _resolver;
  mutable ImportTransactionContext _trxContext;
  std::unique_ptr<transaction::Methods> _trx;
};

}  // namespace

AqlDocumentTransformer::AqlDocumentTransformer(std::string const& queryString,
                                               velocypack::Slice userBindVars,
                                               TRI_vocbase_t& vocbase)
    : _queryContext(nullptr), _expressionContext(nullptr) {
  // 1. Create a lightweight QueryContext with our own no-op transaction.
  //    This follows the StandaloneCalculation / ComputedValues pattern
  //    but avoids the DatabaseFeature dependency.
  _queryContext = std::make_unique<ImportQueryContext>(vocbase);

  // 2. Parse the query string.
  Ast* ast = _queryContext->ast();
  auto qs = QueryString(queryString);
  Parser parser(*_queryContext, *ast, qs);
  // Do NOT force inline ternaries — unlike ComputedValues, we support
  // top-level LET nodes, so ternary extraction into LETs is fine.
  parser.parse();

  // 3. Replace @doc (and user bind params) with temporary variables.
  //    This MUST happen before validateAndDecompose, because that step
  //    captures pointers to expression subtrees. traverseAndModify replaces
  //    child pointers in parent nodes, so any pointer captured before the
  //    replacement would still reference the old (unreplaced) parameter node.
  {
    AstNode* root = const_cast<AstNode*>(ast->root());

    // Validate user bind variables
    if (!userBindVars.isNone() && !userBindVars.isNull()) {
      if (!userBindVars.isObject()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "--custom-query-bindvars must be a JSON object");
      }
      for (auto it : velocypack::ObjectIterator(userBindVars)) {
        if (it.key.copyString() == docParameterName) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "'doc' is a reserved bind variable name in "
              "--custom-query-bindvars");
        }
      }
    }

    ast->scopes()->start(AQL_SCOPE_MAIN);
    _docVariable = ast->variables()->createTemporaryVariable();

    // Create variables for user bind params
    std::vector<std::string> userBindNames;
    if (!userBindVars.isNone() && !userBindVars.isNull()) {
      for (auto it : velocypack::ObjectIterator(userBindVars)) {
        auto key = it.key.copyString();
        userBindNames.push_back(key);
        auto* var = ast->variables()->createTemporaryVariable();
        UserBindVar ubv;
        ubv.variable = var;
        ubv.value.add(it.value);
        _userBindVars.push_back(std::move(ubv));
      }
    }

    // Traverse the AST and replace parameter nodes with variable references
    ast->traverseAndModify(root, [&](AstNode* node) -> AstNode* {
      if (node->type == NODE_TYPE_PARAMETER) {
        auto name = node->getStringView();
        if (name == docParameterName) {
          return ast->createNodeReference(_docVariable);
        }
        for (size_t i = 0; i < userBindNames.size(); ++i) {
          if (name == userBindNames[i]) {
            return ast->createNodeReference(_userBindVars[i].variable);
          }
        }
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED,
            absl::StrCat("unknown bind parameter '@", name, "'"));
      }
      return node;
    });

    ast->scopes()->endCurrent();
  }

  // 4. Validate and optimize the AST (after parameter replacement).
  ast->validateAndOptimize(
      _queryContext->trxForOptimization(),
      {.optimizeNonCacheable = false, .optimizeFunctionCalls = false});

  // 5. Validate and decompose: walk the AST root, classify top-level
  //    statements, reject disallowed constructs. Expression node pointers
  //    are captured here — parameters are already replaced at this point.
  {
    AstNode* root = const_cast<AstNode*>(ast->root());
    TRI_ASSERT(root != nullptr);
    TRI_ASSERT(root->type == NODE_TYPE_ROOT);
    validateAndDecompose(root);
  }

  // 6. Compile Expression objects from the decomposed AST.
  compileExpressions();

  // 8. Set up the expression context.
  _trx = &_queryContext->trxForOptimization();
  _expressionContext =
      std::make_unique<ImportExpressionContext>(*_trx, *_queryContext);

  // 9. Pre-bind constant user bind variables.
  for (auto& ubv : _userBindVars) {
    _expressionContext->setVariable(ubv.variable, ubv.value.slice());
  }

  LOG_TOPIC("f3a1b", INFO, Logger::FIXME)
      << "custom query transformer initialized successfully ("
      << _letBindings.size() << " LET(s), " << _filterExpressions.size()
      << " FILTER(s))";
}

AqlDocumentTransformer::~AqlDocumentTransformer() = default;

void AqlDocumentTransformer::validateAndDecompose(AstNode* root) {
  bool hasReturn = false;
  size_t numMembers = root->numMembers();

  for (size_t i = 0; i < numMembers; ++i) {
    AstNode* node = root->getMemberUnchecked(i);
    TRI_ASSERT(node != nullptr);

    switch (node->type) {
      case NODE_TYPE_LET: {
        // LET var = expr
        // The LET node has 2 members: [0] = variable, [1] = expression
        TRI_ASSERT(node->numMembers() == 2);
        auto* varNode = node->getMember(0);
        auto* exprNode = node->getMember(1);

        // Validate no subqueries in the expression (Phase 1)
        validateExpressionTree(exprNode);

        auto* variable = static_cast<Variable*>(varNode->getData());
        TRI_ASSERT(variable != nullptr);

        LetBinding binding;
        binding.variable = variable;
        binding.expressionNode = exprNode;
        // Expression will be compiled in compileExpressions()
        _letBindings.push_back(std::move(binding));
        break;
      }

      case NODE_TYPE_FILTER: {
        // FILTER expr
        TRI_ASSERT(node->numMembers() == 1);
        auto* exprNode = node->getMember(0);

        // Validate no subqueries in the expression (Phase 1)
        validateExpressionTree(exprNode);

        FilterExpr fe;
        fe.expressionNode = exprNode;
        _filterExpressions.push_back(std::move(fe));
        break;
      }

      case NODE_TYPE_RETURN: {
        if (i != numMembers - 1) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "RETURN must be the last statement in --custom-query");
        }
        if (hasReturn) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "only one RETURN statement is allowed in --custom-query");
        }
        hasReturn = true;

        TRI_ASSERT(node->numMembers() == 1);
        _returnExpressionNode = node->getMember(0);

        // Validate no subqueries in the expression (Phase 1)
        validateExpressionTree(_returnExpressionNode);
        break;
      }

      default:
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            absl::StrCat(
                "statement type '", node->getTypeString(),
                "' is not allowed in --custom-query. "
                "Only LET, FILTER, and RETURN are supported at the top level"));
    }
  }

  if (!hasReturn) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "--custom-query must contain a RETURN statement");
  }
}

void AqlDocumentTransformer::validateExpressionTree(AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  // Reject subqueries (Phase 1 restriction)
  if (node->type == NODE_TYPE_SUBQUERY) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "subqueries are not supported in --custom-query (Phase 1). "
        "Use only expressions, not FOR/SORT/COLLECT inside parentheses");
  }

  // Reject collection/view references
  if (node->type == NODE_TYPE_COLLECTION || node->type == NODE_TYPE_VIEW) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "collection and view access is not allowed in --custom-query");
  }

  // Reject FOR at any level (Phase 1)
  if (node->type == NODE_TYPE_FOR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "FOR is not allowed in --custom-query expressions (Phase 1)");
  }

  // Validate function calls — same restrictions as ComputedValues/Analyzers
  if (node->type == NODE_TYPE_FCALL) {
    auto* func = static_cast<Function*>(node->getData());
    TRI_ASSERT(func != nullptr);

    if (!func->hasFlag(Function::Flags::CanRunOnDBServerCluster) ||
        !func->hasFlag(Function::Flags::CanRunOnDBServerOneShard) ||
        func->hasFlag(Function::Flags::Internal) ||
        func->hasFlag(Function::Flags::CanReadDocuments) ||
        !func->hasFlag(Function::Flags::CanUseInAnalyzer)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          absl::StrCat("function '", func->name,
                       "' cannot be used in --custom-query. "
                       "Only deterministic, side-effect-free functions "
                       "are allowed"));
    }
  }

  // Recurse into children
  size_t n = node->numMembers();
  for (size_t i = 0; i < n; ++i) {
    validateExpressionTree(node->getMemberUnchecked(i));
  }
}

void AqlDocumentTransformer::compileExpressions() {
  Ast* ast = _queryContext->ast();

  for (auto& binding : _letBindings) {
    binding.expression =
        std::make_unique<Expression>(ast, binding.expressionNode);
    binding.expression->prepareForExecution();
  }

  for (auto& fe : _filterExpressions) {
    fe.expression = std::make_unique<Expression>(ast, fe.expressionNode);
    fe.expression->prepareForExecution();
  }

  TRI_ASSERT(_returnExpressionNode != nullptr);
  _returnExpression = std::make_unique<Expression>(ast, _returnExpressionNode);
  _returnExpression->prepareForExecution();
}

TransformResult AqlDocumentTransformer::transform(velocypack::Slice doc) {
  TRI_ASSERT(_expressionContext != nullptr);
  TRI_ASSERT(doc.isObject());

  // Clear per-document state
  _materializedValues.clear();

  try {
    // 1. Bind @doc to the current document.
    _expressionContext->setVariable(_docVariable, doc);

    // 2. Execute LET bindings in order.
    for (auto& binding : _letBindings) {
      bool mustDestroy = false;
      AqlValue result =
          binding.expression->execute(_expressionContext.get(), mustDestroy);
      AqlValueGuard guard(result, mustDestroy);

      // Materialize the result to VPack so it stays alive for subsequent
      // expressions. setVariable() stores a Slice that points into the
      // Builder's buffer, so we keep the Builder alive in
      // _materializedValues.
      MaterializedValue mv;
      mv.variable = binding.variable;
      auto const& vopts = _expressionContext->trx().vpackOptions();
      AqlValueMaterializer materializer(&vopts);
      mv.builder.add(materializer.slice(result));
      _expressionContext->setVariable(binding.variable, mv.builder.slice());
      _materializedValues.push_back(std::move(mv));
    }

    // 3. Evaluate FILTER expressions. If any evaluates to false, skip.
    for (auto& fe : _filterExpressions) {
      bool mustDestroy = false;
      AqlValue filterResult =
          fe.expression->execute(_expressionContext.get(), mustDestroy);
      AqlValueGuard guard(filterResult, mustDestroy);

      if (!filterResult.toBoolean()) {
        // Clean up and skip
        _expressionContext->clearVariable(_docVariable);
        for (auto& mv : _materializedValues) {
          _expressionContext->clearVariable(mv.variable);
        }
        return {TransformAction::kSkip, {}, {}};
      }
    }

    // 4. Evaluate RETURN expression.
    bool mustDestroy = false;
    AqlValue returnResult =
        _returnExpression->execute(_expressionContext.get(), mustDestroy);
    AqlValueGuard guard(returnResult, mustDestroy);

    // 5. Materialize result to VPack.
    TransformResult tr;
    tr.action = TransformAction::kEmit;

    auto const& vopts = _expressionContext->trx().vpackOptions();
    AqlValueMaterializer materializer(&vopts);
    tr.result.add(materializer.slice(returnResult));

    // 6. Validate result type.
    auto resultSlice = tr.result.slice();
    if (resultSlice.isNull()) {
      tr.action = TransformAction::kSkip;
    } else if (resultSlice.isArray()) {
      // Array return: fan-out — each element becomes a separate document.
      // Validate that every element is an object. Null elements are skipped.
      bool allValid = true;
      for (auto it : velocypack::ArrayIterator(resultSlice)) {
        if (!it.isObject() && !it.isNull()) {
          tr.action = TransformAction::kError;
          tr.error = absl::StrCat(
              "RETURN array element must be an object or null; got ",
              it.typeName());
          allValid = false;
          break;
        }
      }
      if (allValid) {
        tr.action = TransformAction::kEmitMultiple;
      }
    } else if (!resultSlice.isObject()) {
      tr.action = TransformAction::kError;
      tr.error = absl::StrCat(
          "RETURN expression must evaluate to an object, array, or null; got ",
          resultSlice.typeName());
    }

    // 7. Cleanup per-document variable bindings.
    _expressionContext->clearVariable(_docVariable);
    for (auto& mv : _materializedValues) {
      _expressionContext->clearVariable(mv.variable);
    }

    return tr;

  } catch (basics::Exception const& ex) {
    // Clean up on exception
    _expressionContext->clearVariable(_docVariable);
    for (auto& mv : _materializedValues) {
      _expressionContext->clearVariable(mv.variable);
    }
    return {TransformAction::kError, {}, ex.message()};
  } catch (std::exception const& ex) {
    _expressionContext->clearVariable(_docVariable);
    for (auto& mv : _materializedValues) {
      _expressionContext->clearVariable(mv.variable);
    }
    return {TransformAction::kError, {}, ex.what()};
  }
}

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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Expression.h"
#include "IResearch/IResearchExpressionContext.h"

#include "search/filter.hpp"

namespace arangodb {

namespace aql {
struct AstNode;
class Ast;
class ExecutionPlan;
}  // namespace aql

namespace iresearch {

struct QueryContext;

struct ExpressionCompilationContext {
  bool operator==(ExpressionCompilationContext const& rhs) const noexcept {
    return ast == rhs.ast && node == rhs.node;
  }

  explicit operator bool() const noexcept { return ast && node; }

  aql::Ast* ast{};
  std::shared_ptr<aql::AstNode> node{};
};

struct ExpressionExecutionContext final : irs::attribute {
  static const std::string_view type_name() noexcept {
    return "arangodb::iresearch::ExpressionExecutionContext";
  }

  ExpressionExecutionContext() = default;

  ExpressionExecutionContext(ViewExpressionContextBase& ctx) noexcept
      : ctx(&ctx) {}

  explicit operator bool() const noexcept { return ctx; }

  // FIXME change 'ctx' to be 'arangodb::aql::ExpressionContext'
  // once IResearchView will be able to evaluate epxressions
  // with loop variable in SEARCH expressions
  ViewExpressionContextBase* ctx{};
};

// Carries the cancellation token to filters, e.g. to stop long regex matches.
struct CancellationAttribute final : irs::attribute {
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::CancellationAttribute";
  }
  ICancellationToken const* token{};
};

// Provides the attributes filters may need at execution time.
struct FilterCtx final : irs::attribute_provider {
  FilterCtx(ViewExpressionContextBase* ctx,
            ICancellationToken const* token) noexcept {
    _execCtx.ctx = ctx;
    _cancellation.token = token;
  }

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<ExpressionExecutionContext>::id()) {
      return &_execCtx;
    }
    if (type == irs::type<CancellationAttribute>::id()) {
      return &_cancellation;
    }
    return nullptr;
  }

  ExpressionExecutionContext _execCtx;
  CancellationAttribute _cancellation;
};

// User-side filter based on arbitrary ArangoDB `Expression`.
class ByExpression final : public irs::FilterWithBoost {
 public:
  static const std::string_view type_name() noexcept {
    return "arangodb::iresearch::ByExpression";
  }

  ByExpression() noexcept = default;

  void init(QueryContext const& ctx, aql::AstNode& node) noexcept;

  void init(QueryContext const& ctx,
            std::shared_ptr<aql::AstNode>&& node) noexcept;

  irs::type_info::type_id type() const noexcept final {
    return irs::type<ByExpression>::id();
  }

  irs::filter::prepared::ptr prepare(
      irs::PrepareContext const& ctx) const final;

  ExpressionCompilationContext const& context() const noexcept { return _ctx; }

  explicit operator bool() const noexcept { return bool(_ctx); }

 private:
  bool equals(irs::filter const& rhs) const noexcept final;

  ExpressionCompilationContext _ctx;
  std::string _allColumn;
};

}  // namespace iresearch
}  // namespace arangodb

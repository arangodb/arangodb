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
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
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

  bool operator!=(ExpressionCompilationContext const& rhs) const noexcept {
    return !(*this == rhs);
  }

  explicit operator bool() const noexcept { return ast && node; }

  size_t hash() const noexcept;

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

// User-side filter based on arbitrary ArangoDB `Expression`.
class ByExpression final : public irs::filter {
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

  using irs::filter::prepare;

  irs::filter::prepared::ptr prepare(
      irs::IndexReader const& index, irs::Scorers const& order,
      irs::score_t filter_boost,
      irs::attribute_provider const* ctx) const final;

  size_t hash() const noexcept final;

  ExpressionCompilationContext const& context() const noexcept { return _ctx; }

  explicit operator bool() const noexcept { return bool(_ctx); }

 private:
  bool equals(irs::filter const& rhs) const noexcept final;

  ExpressionCompilationContext _ctx;
  std::string _allColumn;
};

}  // namespace iresearch
}  // namespace arangodb

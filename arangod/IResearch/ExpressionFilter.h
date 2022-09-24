////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
  static const irs::string_ref type_name() noexcept {
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

class HasNested {
 public:
  explicit HasNested([[maybe_unused]] bool hasNested) noexcept
#ifdef USE_ENTERPRISE
      : _hasNested{hasNested}
#endif
  {
  }

  bool hasNested() const noexcept {
#ifdef USE_ENTERPRISE
    return _hasNested;
#else
    return false;
#endif
  }

 protected:
  ~HasNested() = default;

 private:
#ifdef USE_ENTERPRISE
  bool _hasNested;
#endif
};

// User-side filter based on arbitrary ArangoDB `Expression`.
class ByExpression final : public irs::filter, public HasNested {
 public:
  static const irs::string_ref type_name() noexcept {
    return "arangodb::iresearch::ByExpression";
  }

  ByExpression() noexcept;

  void init(aql::Ast& ast, aql::AstNode& node) noexcept {
    _ctx.ast = &ast;
    _ctx.node.reset(&node, [](aql::AstNode*) {});
  }

  void init(aql::Ast& ast, std::shared_ptr<aql::AstNode>&& node) noexcept {
    _ctx.ast = &ast;
    _ctx.node = std::move(node);
  }

  using irs::filter::prepare;

  virtual irs::filter::prepared::ptr prepare(
      irs::index_reader const& index, irs::Order const& ord, irs::score_t boost,
      irs::attribute_provider const* ctx) const override;

  virtual size_t hash() const noexcept override;

  ExpressionCompilationContext const& context() const noexcept { return _ctx; }

  explicit operator bool() const noexcept { return bool(_ctx); }

  using HasNested::hasNested;
  void hasNested([[maybe_unused]] bool hasNested) noexcept {
#ifdef USE_ENTERPRISE
    static_cast<HasNested&>(*this) = HasNested{hasNested};
#endif
  }

 protected:
  virtual bool equals(irs::filter const& rhs) const noexcept override;

 private:
  ExpressionCompilationContext _ctx;
};

}  // namespace iresearch
}  // namespace arangodb

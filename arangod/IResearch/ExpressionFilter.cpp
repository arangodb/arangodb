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

#include "ExpressionFilter.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/AstNode.h"
#include "IResearch/IResearchFilterFactoryCommon.h"

#include "formats/empty_term_reader.hpp"
#include "search/all_iterator.hpp"
#include "utils/hash_utils.hpp"
#include "utils/memory.hpp"

#include <type_traits>

namespace arangodb::iresearch {
namespace {

class NondeterministicExpressionIteratorBase : public irs::doc_iterator {
 public:
  virtual ~NondeterministicExpressionIteratorBase() noexcept { destroy(); }

 protected:
  NondeterministicExpressionIteratorBase(
      ExpressionCompilationContext const& cctx,
      ExpressionExecutionContext const& ectx)
      : _expr{cctx.ast, cctx.node.get()}, _ctx{ectx} {
    TRI_ASSERT(_ctx.ctx);
  }

  bool evaluate() {
    destroy();  // destroy old value before assignment
    _val = _expr.execute(_ctx.ctx, _destroy);
    return _val.toBoolean();
  }

 private:
  IRS_FORCE_INLINE void destroy() noexcept {
    if (_destroy) {
      _val.destroy();
    }
  }

  aql::Expression _expr;
  aql::AqlValue _val;
  ExpressionExecutionContext _ctx;
  bool _destroy{false};
};

class NondeterministicExpressionIterator
    : public NondeterministicExpressionIteratorBase {
 public:
  NondeterministicExpressionIterator(irs::doc_iterator::ptr&& it,
                                     ExpressionCompilationContext const& cctx,
                                     ExpressionExecutionContext const& ectx)
      : NondeterministicExpressionIteratorBase{cctx, ectx},
        _it{std::move(it)},
        _doc{irs::get<irs::document>(*_it)} {
    TRI_ASSERT(_it);
    TRI_ASSERT(_doc);
  }

  bool next() final {
    while (_it->next()) {
      if (evaluate()) {
        return true;
      }
    }
    return false;
  }

  irs::attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    return _it->get_mutable(id);
  }

  irs::doc_id_t seek(irs::doc_id_t target) final {
    auto const doc = _it->seek(target);

    if (irs::doc_limits::eof(doc) || evaluate()) {
      return doc;
    }

    next();
    return _doc->value;
  }

  irs::doc_id_t value() const noexcept final { return _doc->value; }

 private:
  irs::doc_iterator::ptr _it;
  irs::document const* _doc;
};

class ExpressionQuery : public irs::filter::prepared {
 public:
  void visit(irs::SubReader const& segment, irs::PreparedStateVisitor& visitor,
             irs::score_t boost) const final {
    return _allQuery->visit(segment, visitor, boost);
  }

 protected:
  explicit ExpressionQuery(ExpressionCompilationContext const& ctx,
                           irs::filter::prepared::ptr&& allQuery) noexcept
      : irs::filter::prepared{allQuery->boost()},
        _allQuery{std::move(allQuery)},
        _ctx{ctx} {
    TRI_ASSERT(_allQuery);
  }

  irs::filter::prepared::ptr _allQuery;
  ExpressionCompilationContext _ctx;
};

class NondeterministicExpressionQuery : public ExpressionQuery {
 public:
  explicit NondeterministicExpressionQuery(
      ExpressionCompilationContext const& ctx,
      irs::filter::prepared::ptr&& allQuery) noexcept
      : ExpressionQuery{ctx, std::move(allQuery)} {}

  irs::doc_iterator::ptr execute(irs::ExecutionContext const& ctx) const final {
    if (ADB_UNLIKELY(!ctx.ctx)) {
      // no context provided
      return irs::doc_iterator::empty();
    }

    auto const* execCtx = irs::get<ExpressionExecutionContext>(*ctx.ctx);

    if (!execCtx || !static_cast<bool>(*execCtx)) {
      // no execution context provided
      return irs::doc_iterator::empty();
    }

    // set expression for troubleshooting purposes
    execCtx->ctx->_expr = _ctx.node.get();

    return irs::memory::make_managed<NondeterministicExpressionIterator>(
        _allQuery->execute(ctx), _ctx, *execCtx);
  }
};

class DeterministicExpressionQuery : public ExpressionQuery {
 public:
  explicit DeterministicExpressionQuery(
      ExpressionCompilationContext const& ctx,
      irs::filter::prepared::ptr&& allQuery) noexcept
      : ExpressionQuery{ctx, std::move(allQuery)} {}

  irs::doc_iterator::ptr execute(irs::ExecutionContext const& ctx) const final {
    if (ADB_UNLIKELY(!ctx.ctx)) {
      // no context provided
      return irs::doc_iterator::empty();
    }

    auto const* execCtx = irs::get<ExpressionExecutionContext>(*ctx.ctx);

    if (!execCtx || !static_cast<bool>(*execCtx)) {
      // no execution context provided
      return irs::doc_iterator::empty();
    }

    // set expression for troubleshooting purposes
    execCtx->ctx->_expr = _ctx.node.get();

    aql::Expression expr{_ctx.ast, _ctx.node.get()};
    bool mustDestroy = false;
    auto value = expr.execute(execCtx->ctx, mustDestroy);
    aql::AqlValueGuard guard{value, mustDestroy};

    if (value.toBoolean()) {
      return _allQuery->execute(ctx);
    }

    return irs::doc_iterator::empty();
  }
};

}  // namespace

size_t ExpressionCompilationContext::hash() const noexcept {
  return irs::hash_combine(
      irs::hash_combine(1610612741, aql::AstNodeValueHash()(node.get())), ast);
}

void ByExpression::init(QueryContext const& ctx, aql::AstNode& node) noexcept {
  return init(ctx, std::shared_ptr<aql::AstNode>{&node, [](aql::AstNode*) {}});
}

void ByExpression::init(QueryContext const& ctx,
                        std::shared_ptr<aql::AstNode>&& node) noexcept {
  _ctx.ast = ctx.ast;
  _ctx.node = std::move(node);
  _allColumn = makeAllColumn(ctx);
}

bool ByExpression::equals(irs::filter const& rhs) const noexcept {
  if (!irs::filter::equals(rhs)) {
    return false;
  }
  auto const& impl = static_cast<ByExpression const&>(rhs);
  return _ctx == impl._ctx && _allColumn == impl._allColumn;
}

size_t ByExpression::hash() const noexcept { return _ctx.hash(); }

irs::filter::prepared::ptr ByExpression::prepare(
    irs::IndexReader const& index, irs::Scorers const& order,
    irs::score_t filter_boost, irs::attribute_provider const* ctx) const {
  if (!bool(*this)) {
    // uninitialized filter
    return irs::filter::prepared::empty();
  }

  auto allQuery =
      makeAll(_allColumn)
          ->prepare(index, order, this->boost() * filter_boost, ctx);

  if (ADB_UNLIKELY(!allQuery)) {
    return irs::filter::prepared::empty();
  }

  if (!_ctx.node->isDeterministic()) {
    // non-deterministic expression, make non-deterministic query
    return irs::memory::make_managed<NondeterministicExpressionQuery>(
        _ctx, std::move(allQuery));
  }

  auto* execCtx = ctx ? irs::get<ExpressionExecutionContext>(*ctx) : nullptr;

  if (!execCtx || !static_cast<bool>(*execCtx)) {
    // no execution context provided, make deterministic query
    return irs::memory::make_managed<DeterministicExpressionQuery>(
        _ctx, std::move(allQuery));
  }

  // set expression for troubleshooting purposes
  execCtx->ctx->_expr = _ctx.node.get();

  // evaluate expression
  bool mustDestroy = false;
  aql::Expression expr{_ctx.ast, _ctx.node.get()};
  auto value = expr.execute(execCtx->ctx, mustDestroy);
  aql::AqlValueGuard guard{value, mustDestroy};

  if (!value.toBoolean()) {
    return irs::filter::prepared::empty();
  }

  return allQuery;
}

}  // namespace arangodb::iresearch

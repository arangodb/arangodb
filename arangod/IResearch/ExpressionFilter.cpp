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

#include "ExpressionFilter.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/AstNode.h"

#include "formats/empty_term_reader.hpp"
#include "search/all_filter.hpp"
#include "search/all_iterator.hpp"
#include "utils/hash_utils.hpp"
#include "utils/memory.hpp"

#include <type_traits>

namespace {

template<typename T>
inline irs::filter::prepared::ptr compileQuery(
    arangodb::iresearch::ExpressionCompilationContext const& ctx,
    irs::index_reader const& index, irs::Order const& order,
    irs::score_t boost) {
  typedef
      typename std::enable_if<std::is_base_of<irs::filter::prepared, T>::value,
                              T>::type type_t;

  irs::bstring stats(order.stats_size(), 0);
  auto* stats_buf = const_cast<irs::byte_type*>(stats.data());

  // skip filed-level/term-level statistics because there are no fields/terms
  irs::PrepareCollectors(order.buckets(), stats_buf, index);

  return irs::memory::make_managed<type_t>(ctx, std::move(stats), boost);
}

class NondeterministicExpressionIterator final : public irs::doc_iterator {
 public:
  NondeterministicExpressionIterator(
      irs::sub_reader const& reader, irs::byte_type const* stats,
      irs::Order const& order, uint64_t docs_count,
      arangodb::iresearch::ExpressionCompilationContext const& cctx,
      arangodb::iresearch::ExpressionExecutionContext const& ectx,
      irs::score_t boost)
      : max_doc_(irs::doc_id_t(irs::doc_limits::min() + docs_count - 1)),
        expr_(cctx.ast, cctx.node.get()),
        ctx_(ectx) {
    TRI_ASSERT(ctx_.ctx);

    std::get<irs::cost>(attrs_).reset(max_doc_);

    // set scorers
    if (!order.empty()) {
      auto& score = std::get<irs::score>(attrs_);

      score = irs::CompileScore(order.buckets(), reader,
                                irs::empty_term_reader{docs_count}, stats,
                                *this, boost);
    }
  }

  virtual ~NondeterministicExpressionIterator() noexcept { destroy(); }

  bool next() override {
    auto& doc = std::get<irs::document>(attrs_);
    return !irs::doc_limits::eof(seek(doc.value + 1));
  }

  irs::attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    return irs::get_mutable(attrs_, id);
  }

  irs::doc_id_t seek(irs::doc_id_t target) override {
    auto& doc = std::get<irs::document>(attrs_);

    while (target <= max_doc_) {
      destroy();  // destroy old value before assignment
      val_ = expr_.execute(ctx_.ctx, destroy_);

      if (val_.toBoolean()) {
        break;
      }

      ++target;
    }

    doc.value = target <= max_doc_ ? target : irs::doc_limits::eof();

    return doc.value;
  }

  irs::doc_id_t value() const noexcept override {
    return std::get<irs::document>(attrs_).value;
  }

 private:
  FORCE_INLINE void destroy() noexcept {
    if (destroy_) {
      val_.destroy();
    }
  }

  using attributes = std::tuple<irs::document, irs::cost, irs::score>;

  irs::doc_id_t max_doc_;  // largest valid doc_id
  attributes attrs_;
  arangodb::aql::Expression expr_;
  arangodb::aql::AqlValue val_;
  arangodb::iresearch::ExpressionExecutionContext ctx_;
  bool destroy_{false};
};

class NondeterministicExpressionQuery final : public irs::filter::prepared {
 public:
  explicit NondeterministicExpressionQuery(
      arangodb::iresearch::ExpressionCompilationContext const& ctx,
      irs::bstring&& stats, irs::score_t boost) noexcept
      : irs::filter::prepared(boost), _ctx(ctx), stats_(std::move(stats)) {}

  irs::doc_iterator::ptr execute(
      irs::ExecutionContext const& ctx) const override {
    if (ADB_UNLIKELY(!ctx.ctx)) {
      // no context provided
      return irs::doc_iterator::empty();
    }

    auto const* execCtx =
        irs::get<arangodb::iresearch::ExpressionExecutionContext>(*ctx.ctx);

    if (!execCtx || !static_cast<bool>(*execCtx)) {
      // no execution context provided
      return irs::doc_iterator::empty();
    }

    // set expression for troubleshooting purposes
    execCtx->ctx->_expr = _ctx.node.get();

    auto& segment = ctx.segment;
    return irs::memory::make_managed<NondeterministicExpressionIterator>(
        segment, stats_.c_str(), ctx.scorers, segment.docs_count(), _ctx,
        *execCtx, boost());
  }

  void visit(irs::sub_reader const&, irs::PreparedStateVisitor&,
             irs::score_t) const override {
    // NOOP
  }

 private:
  arangodb::iresearch::ExpressionCompilationContext _ctx;
  irs::bstring stats_;
};

///////////////////////////////////////////////////////////////////////////////
/// @class DeterministicExpressionQuery
///////////////////////////////////////////////////////////////////////////////
class DeterministicExpressionQuery final : public irs::filter::prepared {
 public:
  explicit DeterministicExpressionQuery(
      arangodb::iresearch::ExpressionCompilationContext const& ctx,
      irs::bstring&& stats, irs::score_t boost) noexcept
      : irs::filter::prepared(boost), _ctx(ctx), stats_(std::move(stats)) {}

  irs::doc_iterator::ptr execute(
      irs::ExecutionContext const& ctx) const override {
    if (ADB_UNLIKELY(!ctx.ctx)) {
      // no context provided
      return irs::doc_iterator::empty();
    }

    auto const* execCtx =
        irs::get<arangodb::iresearch::ExpressionExecutionContext>(*ctx.ctx);

    if (!execCtx || !static_cast<bool>(*execCtx)) {
      // no execution context provided
      return irs::doc_iterator::empty();
    }

    // set expression for troubleshooting purposes
    execCtx->ctx->_expr = _ctx.node.get();

    arangodb::aql::Expression expr(_ctx.ast, _ctx.node.get());
    bool mustDestroy = false;
    auto value = expr.execute(execCtx->ctx, mustDestroy);
    arangodb::aql::AqlValueGuard guard(value, mustDestroy);

    if (value.toBoolean()) {
      auto& segment = ctx.segment;
      return irs::memory::make_managed<irs::all_iterator>(
          segment, stats_.c_str(), ctx.scorers, segment.docs_count(), boost());
    }

    return irs::doc_iterator::empty();
  }

  void visit(irs::sub_reader const&, irs::PreparedStateVisitor&,
             irs::score_t) const override {
    // NOOP
  }

 private:
  arangodb::iresearch::ExpressionCompilationContext _ctx;
  irs::bstring stats_;
};

}  // namespace

namespace arangodb {
namespace iresearch {

size_t ExpressionCompilationContext::hash() const noexcept {
  return irs::hash_combine(
      irs::hash_combine(1610612741,
                        arangodb::aql::AstNodeValueHash()(node.get())),
      ast);
}

ByExpression::ByExpression() noexcept
    : irs::filter(irs::type<ByExpression>::get()) {}

bool ByExpression::equals(irs::filter const& rhs) const noexcept {
  auto const& typed = static_cast<ByExpression const&>(rhs);
  return irs::filter::equals(rhs) && _ctx == typed._ctx;
}

size_t ByExpression::hash() const noexcept { return _ctx.hash(); }

irs::filter::prepared::ptr ByExpression::prepare(
    irs::index_reader const& index, irs::Order const& order,
    irs::score_t filter_boost, irs::attribute_provider const* ctx) const {
  if (!bool(*this)) {
    // uninitialized filter
    return irs::filter::prepared::empty();
  }

  filter_boost *= this->boost();

  if (!_ctx.node->isDeterministic()) {
    // non-deterministic expression, make non-deterministic query
    return compileQuery<NondeterministicExpressionQuery>(_ctx, index, order,
                                                         filter_boost);
  }

  auto* execCtx =
      ctx ? irs::get<arangodb::iresearch::ExpressionExecutionContext>(*ctx)
          : nullptr;

  if (!execCtx || !static_cast<bool>(*execCtx)) {
    // no execution context provided, make deterministic query
    return compileQuery<DeterministicExpressionQuery>(_ctx, index, order,
                                                      filter_boost);
  }

  // set expression for troubleshooting purposes
  execCtx->ctx->_expr = _ctx.node.get();

  // evaluate expression
  bool mustDestroy = false;
  arangodb::aql::Expression expr(_ctx.ast, _ctx.node.get());
  auto value = expr.execute(execCtx->ctx, mustDestroy);
  arangodb::aql::AqlValueGuard guard(value, mustDestroy);

  return value.toBoolean() ? irs::all().prepare(index, order, filter_boost)
                           : irs::filter::prepared::empty();
}

}  // namespace iresearch
}  // namespace arangodb

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
#include "IResearch/IResearchFilterFactoryCommon.hpp"

#include "formats/empty_term_reader.hpp"
#include "search/all_iterator.hpp"
#include "utils/hash_utils.hpp"
#include "utils/memory.hpp"

#include <type_traits>

namespace arangodb {
namespace iresearch {

irs::doc_iterator::ptr makeAllIterator(irs::sub_reader const& segment,
                                       irs::bytes_ref stats,
                                       const irs::Order& scorers,
                                       irs::score_t boost, bool);

#ifndef USE_ENTERPRISE
irs::doc_iterator::ptr makeAllIterator(irs::sub_reader const& segment,
                                       irs::bytes_ref stats,
                                       const irs::Order& scorers,
                                       irs::score_t boost, bool) {
  return irs::memory::make_managed<irs::all_iterator>(
      segment, stats.c_str(), scorers, segment.docs_count(), boost);
}
#endif

namespace {

template<typename T>
irs::filter::prepared::ptr compileQuery(ExpressionCompilationContext const& ctx,
                                        irs::index_reader const& index,
                                        irs::Order const& order,
                                        irs::score_t boost, bool hasNested) {
  using type_t =
      typename std::enable_if_t<std::is_base_of_v<irs::filter::prepared, T>, T>;

  irs::bstring stats(order.stats_size(), 0);
  auto* stats_buf = const_cast<irs::byte_type*>(stats.data());

  // skip filed-level/term-level statistics because there are no fields/terms
  irs::PrepareCollectors(order.buckets(), stats_buf, index);

  return irs::memory::make_managed<type_t>(ctx, std::move(stats), boost,
                                           hasNested);
}

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
  FORCE_INLINE void destroy() noexcept {
    if (_destroy) {
      _val.destroy();
    }
  }

  aql::Expression _expr;
  aql::AqlValue _val;
  ExpressionExecutionContext _ctx;
  bool _destroy{false};
};

class NondeterministicExpressionIterator final
    : public NondeterministicExpressionIteratorBase {
 public:
  NondeterministicExpressionIterator(irs::doc_iterator::ptr&& it,
                                     ExpressionCompilationContext const& cctx,
                                     ExpressionExecutionContext const& ectx)
      : NondeterministicExpressionIteratorBase{cctx, ectx},
        _it{std::move(it)},
        _doc{irs::get<irs::document>(it)} {
    TRI_ASSERT(_it);
    TRI_ASSERT(_doc);
  }

  bool next() override {
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

  irs::doc_id_t seek(irs::doc_id_t target) override {
    auto const doc = _it->seek(target);

    if (irs::doc_limits::eof(doc) || evaluate()) {
      return doc;
    }

    next();
    return _doc->value;
  }

  irs::doc_id_t value() const noexcept override { return _doc->value; }

 private:
  irs::doc_iterator::ptr _it;
  irs::document const* _doc;
};

class NondeterministicExpressionAllIterator final
    : public NondeterministicExpressionIteratorBase {
 public:
  NondeterministicExpressionAllIterator(
      irs::sub_reader const& reader, irs::byte_type const* stats,
      irs::Order const& order, uint64_t docs_count,
      ExpressionCompilationContext const& cctx,
      ExpressionExecutionContext const& ectx, irs::score_t boost)
      : NondeterministicExpressionIteratorBase{cctx, ectx},
        max_doc_{irs::doc_id_t(irs::doc_limits::min() + docs_count - 1)} {
    std::get<irs::cost>(attrs_).reset(max_doc_);

    // set scorers
    if (!order.empty()) {
      auto& score = std::get<irs::score>(attrs_);

      score = irs::CompileScore(order.buckets(), reader,
                                irs::empty_term_reader{docs_count}, stats,
                                *this, boost);
    }
  }

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
      if (evaluate()) {
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
  using attributes = std::tuple<irs::document, irs::cost, irs::score>;

  irs::doc_id_t max_doc_;  // largest valid doc_id
  attributes attrs_;
};

irs::doc_iterator::ptr makeNonDeterministicAllIterator(
    ExpressionCompilationContext const& cctx,
    ExpressionExecutionContext const& ectx, irs::sub_reader const& segment,
    irs::bytes_ref stats, const irs::Order& scorers, irs::score_t boost,
    [[maybe_unused]] bool hasNested) {
#ifdef USE_ENTERPRISE
  if (hasNested) {
    return irs::memory::make_managed<NondeterministicExpressionIterator>(
        makeAllIterator(segment, stats, scorers, boost, true), cctx, ectx);
  }
#endif

  return irs::memory::make_managed<NondeterministicExpressionAllIterator>(
      segment, stats.c_str(), scorers, segment.docs_count(), cctx, ectx, boost);
}

class NondeterministicExpressionQuery final : public irs::filter::prepared,
                                              private HasNested {
 public:
  explicit NondeterministicExpressionQuery(
      ExpressionCompilationContext const& ctx, irs::bstring&& stats,
      irs::score_t boost, bool hasNested) noexcept
      : irs::filter::prepared{boost},
        HasNested{hasNested},
        _ctx{ctx},
        _stats{std::move(stats)} {}

  irs::doc_iterator::ptr execute(
      irs::ExecutionContext const& ctx) const override {
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

    return makeNonDeterministicAllIterator(_ctx, *execCtx, ctx.segment, _stats,
                                           ctx.scorers, boost(), hasNested());
  }

  void visit(irs::sub_reader const&, irs::PreparedStateVisitor&,
             irs::score_t) const override {
    // NOOP
  }

 private:
  ExpressionCompilationContext _ctx;
  irs::bstring _stats;
};

class DeterministicExpressionQuery final : public irs::filter::prepared,
                                           private HasNested {
 public:
  explicit DeterministicExpressionQuery(ExpressionCompilationContext const& ctx,
                                        irs::bstring&& stats,
                                        irs::score_t boost,
                                        bool hasNested) noexcept
      : irs::filter::prepared{boost},
        HasNested{hasNested},
        _ctx{ctx},
        _stats{std::move(stats)} {}

  irs::doc_iterator::ptr execute(
      irs::ExecutionContext const& ctx) const override {
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

    aql::Expression expr(_ctx.ast, _ctx.node.get());
    bool mustDestroy = false;
    auto value = expr.execute(execCtx->ctx, mustDestroy);
    aql::AqlValueGuard guard(value, mustDestroy);

    if (value.toBoolean()) {
      return makeAllIterator(ctx.segment, _stats, ctx.scorers, boost(),
                             hasNested());
    }

    return irs::doc_iterator::empty();
  }

  void visit(irs::sub_reader const&, irs::PreparedStateVisitor&,
             irs::score_t) const override {
    // NOOP
  }

 private:
  ExpressionCompilationContext _ctx;
  irs::bstring _stats;
};

}  // namespace

size_t ExpressionCompilationContext::hash() const noexcept {
  return irs::hash_combine(
      irs::hash_combine(1610612741, aql::AstNodeValueHash()(node.get())), ast);
}

ByExpression::ByExpression() noexcept
    : irs::filter{irs::type<ByExpression>::get()}, HasNested{false} {}

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
    return compileQuery<NondeterministicExpressionQuery>(
        _ctx, index, order, filter_boost, hasNested());
  }

  auto* execCtx = ctx ? irs::get<ExpressionExecutionContext>(*ctx) : nullptr;

  if (!execCtx || !static_cast<bool>(*execCtx)) {
    // no execution context provided, make deterministic query
    return compileQuery<DeterministicExpressionQuery>(
        _ctx, index, order, filter_boost, hasNested());
  }

  // set expression for troubleshooting purposes
  execCtx->ctx->_expr = _ctx.node.get();

  // evaluate expression
  bool mustDestroy = false;
  aql::Expression expr(_ctx.ast, _ctx.node.get());
  auto value = expr.execute(execCtx->ctx, mustDestroy);
  aql::AqlValueGuard guard(value, mustDestroy);

  if (!value.toBoolean()) {
    return irs::filter::prepared::empty();
  }

  return makeAll(hasNested())->prepare(index, order, filter_boost);
}

}  // namespace iresearch
}  // namespace arangodb

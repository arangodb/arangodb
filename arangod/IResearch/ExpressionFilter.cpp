////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/AstNode.h"
#include "Aql/AqlValue.h"

#include "IResearch/IResearchViewBlock.h"

#include "search/score_doc_iterators.hpp"
#include "search/all_filter.hpp"
#include "search/all_iterator.hpp"
#include "formats/empty_term_reader.hpp"
#include "utils/hash_utils.hpp"

#include <type_traits>

namespace {

template<typename T>
inline irs::filter::prepared::ptr compileQuery(
   arangodb::iresearch::ExpressionCompilationContext const &ctx,
   irs::index_reader const& index,
   irs::order::prepared const& order,
   irs::boost::boost_t boost) {
  typedef typename std::enable_if<
    std::is_base_of<irs::filter::prepared, T>::value, T
  >::type type_t;

  irs::attribute_store attrs;

  // skip filed-level/term-level statistics because there are no fields/terms
  order.prepare_stats().finish(attrs, index);

  // apply boost
  irs::boost::apply(attrs, boost);

  return irs::filter::prepared::make<type_t>(ctx, std::move(attrs));
}

///////////////////////////////////////////////////////////////////////////////
/// @class NondeterministicExpressionIterator
///////////////////////////////////////////////////////////////////////////////
class NondeterministicExpressionIterator final : public irs::doc_iterator_base {
 public:
  NondeterministicExpressionIterator(
      irs::sub_reader const& reader,
      irs::attribute_store const& prepared_filter_attrs,
      irs::order::prepared const& order,
      uint64_t docs_count,
      arangodb::iresearch::ExpressionCompilationContext const& cctx,
      arangodb::iresearch::ExpressionExecutionContext const& ectx)
    : doc_iterator_base(order),
      max_doc_(irs::doc_id_t(irs::type_limits<irs::type_t::doc_id_t>::min() + docs_count - 1)),
      expr_(cctx.plan, cctx.ast, cctx.node.get()),
      ctx_(ectx) {
    TRI_ASSERT(ctx_.ctx && ctx_.trx);

    // make doc_id accessible via attribute
    attrs_.emplace(doc_);

    // set estimation value
    estimate(max_doc_);

    // set scorers
    scorers_ = ord_->prepare_scorers(
      reader,
      irs::empty_term_reader(docs_count),
      prepared_filter_attrs,
      attributes() // doc_iterator attributes
    );

    prepare_score([this](irs::byte_type* score) {
      scorers_.score(*ord_, score);
    });
  }

  virtual ~NondeterministicExpressionIterator() noexcept {
    destroy();
  }

  virtual bool next() override {
    return !irs::type_limits<irs::type_t::doc_id_t>::eof(
      seek(doc_.value + 1)
    );
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    while (target <= max_doc_) {
      destroy(); // destroy old value before assignment
      val_ = expr_.execute(ctx_.trx, ctx_.ctx, destroy_);

      if (val_.toBoolean()) {
        break;
      }

      ++target;
    }

    doc_.value = target <= max_doc_
      ? target
      : irs::type_limits<irs::type_t::doc_id_t>::eof();

    return doc_.value;
  }

  virtual irs::doc_id_t value() const noexcept override {
    return doc_.value;
  }

 private:
  FORCE_INLINE void destroy() noexcept {
    if (destroy_) {
      val_.destroy();
    }
  }

  irs::document doc_;
  irs::doc_id_t max_doc_; // largest valid doc_id
  irs::order::prepared::scorers scorers_;
  arangodb::aql::Expression expr_;
  arangodb::aql::AqlValue val_;
  arangodb::iresearch::ExpressionExecutionContext ctx_;
  bool destroy_{false};
}; // NondeterministicExpressionIterator

///////////////////////////////////////////////////////////////////////////////
/// @class NondeterministicExpressionQuery
///////////////////////////////////////////////////////////////////////////////
class NondeterministicExpressionQuery final : public irs::filter::prepared {
 public:
  explicit NondeterministicExpressionQuery(
      arangodb::iresearch::ExpressionCompilationContext const& ctx,
      irs::attribute_store&& attrs
  ) noexcept
    : irs::filter::prepared(std::move(attrs)),
      _ctx(ctx) {
  }

  virtual irs::doc_iterator::ptr execute(
      const irs::sub_reader& rdr,
      const irs::order::prepared& order,
      const irs::attribute_view& ctx
  ) const override {
    auto const& execCtx = ctx.get<arangodb::iresearch::ExpressionExecutionContext>();

    if (!execCtx || !static_cast<bool>(*execCtx)) {
      // no execution context provided
      return irs::doc_iterator::empty();
    }

    // set expression for troubleshooting purposes
    execCtx->ctx->_expr = _ctx.node.get();

    return irs::doc_iterator::make<NondeterministicExpressionIterator>(
      rdr,
      attributes(), // prepared_filter attributes
      order,
      rdr.docs_count(),
      _ctx,
      *execCtx
    );
  }

 private:
  arangodb::iresearch::ExpressionCompilationContext _ctx;
}; // NondeterministicExpressionQuery

///////////////////////////////////////////////////////////////////////////////
/// @class DeterministicExpressionQuery
///////////////////////////////////////////////////////////////////////////////
class DeterministicExpressionQuery final : public irs::filter::prepared {
 public:
  explicit DeterministicExpressionQuery(
      arangodb::iresearch::ExpressionCompilationContext const& ctx,
      irs::attribute_store&& attrs
  ) noexcept
    : irs::filter::prepared(std::move(attrs)),
      _ctx(ctx) {
  }

  virtual irs::doc_iterator::ptr execute(
      const irs::sub_reader& segment,
      const irs::order::prepared& order,
      const irs::attribute_view& ctx
  ) const override {
    auto const& execCtx = ctx.get<arangodb::iresearch::ExpressionExecutionContext>();

    if (!execCtx || !static_cast<bool>(*execCtx)) {
      // no execution context provided
      return irs::doc_iterator::empty();
    }

    // set expression for troubleshooting purposes
    execCtx->ctx->_expr = _ctx.node.get();

    arangodb::aql::Expression expr(_ctx.plan, _ctx.ast, _ctx.node.get());
    bool mustDestroy = false;
    auto value = expr.execute(execCtx->trx, execCtx->ctx, mustDestroy);
    arangodb::aql::AqlValueGuard guard(value, mustDestroy);

    if (value.toBoolean()) {
      return irs::doc_iterator::make<irs::all_iterator>(
        segment, attributes(), order, segment.docs_count()
      );
    }

    return irs::doc_iterator::empty();
  }

 private:
  arangodb::iresearch::ExpressionCompilationContext _ctx;
}; // DeterministicExpressionQuery

}

namespace arangodb {
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                     ExpressionCompilationContext implementation
///////////////////////////////////////////////////////////////////////////////

size_t ExpressionCompilationContext::hash() const noexcept {
  return irs::hash_combine(
    irs::hash_combine(
      irs::hash_combine(
        1610612741,
        arangodb::aql::AstNodeValueHash()(node.get())
      ),
      plan
    ),
    ast
  );
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                       ExpressionExecutionContext implementation
///////////////////////////////////////////////////////////////////////////////

DEFINE_ATTRIBUTE_TYPE(ExpressionExecutionContext);

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                     ByExpression implementation
///////////////////////////////////////////////////////////////////////////////

DEFINE_FILTER_TYPE(ByExpression);
DEFINE_FACTORY_DEFAULT(ByExpression);

ByExpression::ByExpression() noexcept
  : irs::filter(ByExpression::type()) {
}

bool ByExpression::equals(irs::filter const& rhs) const noexcept {
  auto const& typed = static_cast<ByExpression const&>(rhs);
  return irs::filter::equals(rhs) && _ctx == typed._ctx;
}

size_t ByExpression::hash() const noexcept {
  return _ctx.hash();
}

irs::filter::prepared::ptr ByExpression::prepare(
    irs::index_reader const& index,
    irs::order::prepared const& order,
    irs::boost::boost_t filter_boost,
    irs::attribute_view const& ctx) const {
  if (!bool(*this)) {
    // uninitialized filter
    return irs::filter::prepared::empty();
  }

  filter_boost *= this->boost();

  if (!_ctx.node->isDeterministic()) {
    // non-deterministic expression, make non-deterministic query
    return compileQuery<NondeterministicExpressionQuery>(
      _ctx, index, order, filter_boost
    );
  }

  auto* execCtx = ctx.get<arangodb::iresearch::ExpressionExecutionContext>().get();

  if (!execCtx || !static_cast<bool>(*execCtx)) {
    // no execution context provided, make deterministic query
    return compileQuery<DeterministicExpressionQuery>(
      _ctx, index, order, filter_boost
    );
  }

  // set expression for troubleshooting purposes
  execCtx->ctx->_expr = _ctx.node.get();

  // evaluate expression
  bool mustDestroy = false;
  arangodb::aql::Expression expr(_ctx.plan, _ctx.ast, _ctx.node.get());
  auto value = expr.execute(execCtx->trx, execCtx->ctx, mustDestroy);
  arangodb::aql::AqlValueGuard guard(value, mustDestroy);

  return value.toBoolean()
    ? irs::all().prepare(index, order, filter_boost)
    : irs::filter::prepared::empty();
}

} // iresearch
} // arangodb

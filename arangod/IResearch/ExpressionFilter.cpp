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

#include "Aql/AstNode.h"
#include "Aql/AqlValue.h"

#include "search/score_doc_iterators.hpp"
#include "search/all_filter.hpp"
#include "formats/empty_term_reader.hpp"

NS_LOCAL

struct ArangoExpressionContext {
  arangodb::aql::ExecutionPlan* plan;
  arangodb::aql::Ast* ast;
  arangodb::aql::AstNode* node;
  arangodb::transaction::Methods* trx;
  arangodb::aql::ExpressionContext* ctx;
}; // ArangoExpressionContext

class NondeterministicExpressionIterator final : public irs::doc_iterator_base {
 public:
  NondeterministicExpressionIterator(
      irs::sub_reader const& reader,
      irs::attribute_store const& prepared_filter_attrs,
      irs::order::prepared const& order,
      uint64_t docs_count,
      ArangoExpressionContext const& ctx)
    : doc_iterator_base(order),
      max_doc_(irs::doc_id_t(irs::type_limits<irs::type_t::doc_id_t>::min() + docs_count - 1)),
      expr_(ctx.plan, ctx.ast, ctx.node),
      ctx_(ctx.ctx),
      trx_(ctx.trx) {
    TRI_ASSERT(ctx_ && trx_);

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
      val_ = expr_.execute(trx_, ctx_, destroy_);

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
  arangodb::aql::ExpressionContext* ctx_;
  arangodb::transaction::Methods* trx_;
  bool destroy_{false};
}; // NondeterministicExpressionIterator

class NondeterministicExpressionQuery final : public irs::filter::prepared {
 public:
  explicit NondeterministicExpressionQuery(
      ArangoExpressionContext const& ctx,
      irs::attribute_store&& attrs
  ) noexcept
    : irs::filter::prepared(std::move(attrs)),
      _ctx(ctx) {
  }

  virtual irs::doc_iterator::ptr execute(
      const irs::sub_reader& rdr,
      const irs::order::prepared& order
  ) const override {
    return irs::doc_iterator::make<NondeterministicExpressionIterator>(
      rdr,
      attributes(), // prepared_filter attributes
      order,
      rdr.docs_count(),
      _ctx
    );
  }

 private:
  ArangoExpressionContext _ctx;
}; // NondeterministicExpressionQuery

FORCE_INLINE size_t hash_combine(size_t seed, size_t v) {
  return seed ^ (v + 0x9e3779b9 + (seed<<6) + (seed>>2));
}

template<typename T>
FORCE_INLINE size_t hash_combine(size_t seed, T const& v) {
  return hash_combine(seed, std::hash<T>()(v));
}

NS_END // LOCAL

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

DEFINE_FILTER_TYPE(ByExpression);
DEFINE_FACTORY_DEFAULT(ByExpression);

ByExpression::ByExpression() noexcept
  : irs::filter(ByExpression::type()) {
}

bool ByExpression::equals(irs::filter const& rhs) const {
  auto const& typed = static_cast<ByExpression const&>(rhs);
  return irs::filter::equals(rhs)
    && _node == typed._node
    && _plan == typed._plan
    && _ast == typed._ast;
}

size_t ByExpression::hash() const {
  return hash_combine(
    hash_combine(
      hash_combine(
        1610612741,
        arangodb::aql::AstNodeValueHash()(_node)
      ),
      _plan
    ),
    _ast
  );
}

irs::filter::prepared::ptr ByExpression::prepare(
    irs::index_reader const& index,
    irs::order::prepared const& order,
    irs::boost::boost_t filter_boost) const {
  if (!bool(*this)) {
    // uninitialized filter
    return irs::filter::prepared::empty();
  }

  filter_boost *= this->boost();

  if (!_node->isDeterministic()) {
    irs::attribute_store attrs;

    // skip filed-level/term-level statistics because there are no fields/terms
    order.prepare_stats().finish(attrs, index);

    // apply boost
    irs::boost::apply(attrs, filter_boost);

    return irs::filter::prepared::make<NondeterministicExpressionQuery>(
      ArangoExpressionContext{ _plan, _ast, _node, _trx, _ctx },
      std::move(attrs)
    );
  }

  arangodb::aql::Expression expr(_plan, _ast, _node);
  bool mustDestroy = false;
  auto value = expr.execute(_trx, _ctx, mustDestroy);
  arangodb::aql::AqlValueGuard guard(value, mustDestroy);

  return value.toBoolean()
    ? irs::all().prepare(index, order, filter_boost)
    : irs::filter::prepared::empty();
}

NS_END // iresearch
NS_END // arangodb

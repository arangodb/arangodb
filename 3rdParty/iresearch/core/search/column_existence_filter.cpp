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

#include "column_existence_filter.hpp"
#include "formats/empty_term_reader.hpp"
#include "index/field_meta.hpp"
#include "search/score_doc_iterators.hpp"
#include "search/disjunction.hpp"

#include <boost/functional/hash.hpp>

NS_LOCAL

class column_existence_iterator final : public irs::score_doc_iterator_base {
 public:
  explicit column_existence_iterator(
      const irs::sub_reader& reader,
      const irs::attribute_store& prepared_filter_attrs,
      irs::columnstore_reader::column_iterator::ptr&& it,
      const irs::order::prepared& ord,
      uint64_t docs_count)
    : score_doc_iterator_base(ord),
      it_(std::move(it)) {
    assert(it_);

    // set estimation value
    est_.value(docs_count);
    attrs_.emplace(est_);

    // make doc_id accessible via attribute
    attrs_.emplace(doc_);

    // set scorers
    scorers_ = ord_->prepare_scorers(
      reader,
      irs::empty_term_reader(docs_count),
      prepared_filter_attrs,
      attributes() // doc_iterator attributes
    );
  }

  virtual bool next() override {
    return it_->next();
  }

  virtual void score() override {
    value(); // ensure doc_id is updated before scoring
    scorers_.score(*ord_, scr_.leak());
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    it_->seek(target);

    return value();
  }

  virtual irs::doc_id_t value() const NOEXCEPT override {
    doc_.value = it_->value().first;

    return doc_.value;
  }

 private:
  mutable irs::document doc_; // modified during value()
  irs::cost est_;
  irs::columnstore_reader::column_iterator::ptr it_;
  irs::order::prepared::scorers scorers_;
}; // column_existence_iterator

class column_existence_query final : public irs::filter::prepared {
 public:
  explicit column_existence_query(
    const std::string& field,
    irs::attribute_store&& attrs
  ): irs::filter::prepared(std::move(attrs)), field_(field) {
  }

  virtual irs::score_doc_iterator::ptr execute(
      const irs::sub_reader& rdr,
      const irs::order::prepared& ord
  ) const override {
    const auto* column = rdr.column_reader(field_);

    if (!column) {
      return irs::score_doc_iterator::empty();
    }

    return irs::score_doc_iterator::make<column_existence_iterator>(
      rdr,
      attributes(), // prepared_filter attributes
      column->iterator(),
      ord,
      column->size()
    );
  }

 private:
  std::string field_;
}; // column_existence_query

class column_prefix_existence_query final : public irs::filter::prepared {
 public:
  explicit column_prefix_existence_query(
    const std::string& prefix,
    irs::attribute_store&& attrs
  ): irs::filter::prepared(std::move(attrs)), prefix_(prefix) {
  }

  virtual irs::score_doc_iterator::ptr execute(
      const irs::sub_reader& rdr,
      const irs::order::prepared& ord
  ) const override {
    auto it = rdr.columns();

    if (!it->seek(prefix_)) {
      // reached the end
      return irs::score_doc_iterator::empty();
    }

    typedef irs::detail::disjunction<
      irs::score_wrapper<column_existence_iterator::ptr>
    > disjunction_t;
    disjunction_t::doc_iterators_t itrs;

    while (irs::starts_with(it->value().name, prefix_)) {
      const auto* column = rdr.column_reader(it->value().id);

      if (!column) {
        continue;
      }

      auto column_it = irs::memory::make_unique<column_existence_iterator>(
        rdr,
        attributes(), // prepared_filter attributes
        column->iterator(),
        ord,
        column->size()
      );

      itrs.emplace_back(std::move(column_it));

      if (!it->next()) {
        break;
      }
    }

    return irs::detail::make_disjunction<disjunction_t>(
      std::move(itrs), 1, ord
    );
  }

 private:
  std::string prefix_;
}; // column_prefix_existence_query

NS_END

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                by_column_existence implementation
// -----------------------------------------------------------------------------

DEFINE_FILTER_TYPE(by_column_existence);
DEFINE_FACTORY_DEFAULT(by_column_existence);

by_column_existence::by_column_existence() NOEXCEPT
  : filter(by_column_existence::type()) {
}

bool by_column_existence::equals(const filter& rhs) const {
  const auto& trhs = static_cast<const by_column_existence&>(rhs);

  return filter::equals(rhs)
    && field_ == trhs.field_
    && prefix_match_ == trhs.prefix_match_;
}

size_t by_column_existence::hash() const {
  size_t seed = 0;
  ::boost::hash_combine(seed, filter::hash());
  ::boost::hash_combine(seed, field_);
  ::boost::hash_combine(seed, prefix_match_);
  return seed;
}

filter::prepared::ptr by_column_existence::prepare(
    const index_reader& reader,
    const order::prepared& order,
    boost_t filter_boost
) const {
  attribute_store attrs;

  // skip filed-level/term-level statistics because there are no fields/terms
  order.prepare_stats().finish(reader, attrs);

  irs::boost::apply(attrs, boost() * filter_boost); // apply boost

  return prefix_match_
    ? filter::prepared::make<column_prefix_existence_query>(field_, std::move(attrs))
    : filter::prepared::make<column_existence_query>(field_, std::move(attrs))
    ;
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

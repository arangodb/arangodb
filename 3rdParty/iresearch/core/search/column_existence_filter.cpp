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

class column_existence_iterator final : public irs::doc_iterator_base {
 public:
  explicit column_existence_iterator(
      const irs::sub_reader& reader,
      const irs::attribute_store& prepared_filter_attrs,
      irs::columnstore_iterator::ptr&& it,
      const irs::order::prepared& ord,
      uint64_t docs_count)
    : doc_iterator_base(ord),
      it_(std::move(it)) {
    assert(it_);
    // make doc_id accessible via attribute
    attrs_.emplace(doc_);

    // set estimation value
    estimate(docs_count);

    // set scorers
    scorers_ = ord_->prepare_scorers(
      reader,
      irs::empty_term_reader(docs_count),
      prepared_filter_attrs,
      attributes() // doc_iterator attributes
    );

    prepare_score([this](irs::byte_type* score) {
      value(); // ensure doc_id is updated before scoring
      scorers_.score(*ord_, score);
    });
  }

  virtual bool next() override {
    return it_->next();
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
  irs::columnstore_iterator::ptr it_;
  irs::order::prepared::scorers scorers_;
}; // column_existence_iterator

class column_existence_query final : public irs::filter::prepared {
 public:
  explicit column_existence_query(
    const std::string& field,
    irs::attribute_store&& attrs
  ): irs::filter::prepared(std::move(attrs)), field_(field) {
  }

  virtual irs::doc_iterator::ptr execute(
      const irs::sub_reader& rdr,
      const irs::order::prepared& ord,
      const irs::attribute_view& /*ctx*/
  ) const override {
    const auto* column = rdr.column_reader(field_);

    if (!column) {
      return irs::doc_iterator::empty();
    }

    return irs::doc_iterator::make<column_existence_iterator>(
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

  virtual irs::doc_iterator::ptr execute(
      const irs::sub_reader& rdr,
      const irs::order::prepared& ord,
      const irs::attribute_view& /*ctx*/
  ) const override {
    auto it = rdr.columns();

    if (!it->seek(prefix_)) {
      // reached the end
      return irs::doc_iterator::empty();
    }

    irs::disjunction::doc_iterators_t itrs;

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

    return irs::make_disjunction<irs::disjunction>(
      std::move(itrs), ord
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
    boost_t filter_boost,
    const attribute_view& /*ctx*/
) const {
  attribute_store attrs;

  // skip filed-level/term-level statistics because there are no fields/terms
  order.prepare_stats().finish(attrs, reader);

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

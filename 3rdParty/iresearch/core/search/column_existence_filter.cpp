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

class column_existence_iterator final : public irs::basic_doc_iterator_base {
 public:
  explicit column_existence_iterator(
      const irs::sub_reader& reader,
      const irs::byte_type* stats,
      irs::doc_iterator::ptr&& it,
      const irs::order::prepared& ord,
      uint64_t docs_count,
      irs::boost_t boost)
    : it_(std::move(it)) {
    assert(it_);

    // make doc_id accessible via attribute
    doc_ = (attrs_.emplace<irs::document>()
              = it_->attributes().get<irs::document>()).get();
    assert(doc_);

    // make doc_payload accessible via attribute
    attrs_.emplace<irs::payload>() =
      it_->attributes().get<irs::payload>();

    // set estimation value
    estimate(docs_count);

    // set scorers
    prepare_score(ord, ord.prepare_scorers(
      reader,
      irs::empty_term_reader(docs_count),
      stats,
      attributes(), // doc_iterator attributes
      boost
    ));
  }

  virtual bool next() override {
    return it_->next();
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    it_->seek(target);

    return value();
  }

  virtual irs::doc_id_t value() const NOEXCEPT override {
    return doc_->value;
  }

 private:
  const irs::document* doc_{};
  irs::doc_iterator::ptr it_;
}; // column_existence_iterator

class column_existence_query final : public irs::filter::prepared {
 public:
  explicit column_existence_query(
      const std::string& field,
      irs::bstring&& stats,
      irs::boost_t boost)
    : irs::filter::prepared(std::move(stats), boost),
      field_(field) {
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
      stats(), // prepared_filter attributes
      column->iterator(),
      ord,
      column->size(),
      boost()
    );
  }

 private:
  std::string field_;
}; // column_existence_query

class column_prefix_existence_query final : public irs::filter::prepared {
 public:
  explicit column_prefix_existence_query(
      const std::string& prefix,
      irs::bstring&& stats,
      irs::boost_t boost)
    : irs::filter::prepared(std::move(stats), boost),
      prefix_(prefix) {
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
        stats(), // prepared_filter attributes
        column->iterator(),
        ord,
        column->size(),
        boost()
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

DEFINE_FILTER_TYPE(by_column_existence)
DEFINE_FACTORY_DEFAULT(by_column_existence)

by_column_existence::by_column_existence() NOEXCEPT
  : filter(by_column_existence::type()) {
}

bool by_column_existence::equals(const filter& rhs) const NOEXCEPT {
  const auto& trhs = static_cast<const by_column_existence&>(rhs);

  return filter::equals(rhs)
    && field_ == trhs.field_
    && prefix_match_ == trhs.prefix_match_;
}

size_t by_column_existence::hash() const NOEXCEPT {
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
  // skip field-level/term-level statistics because there are no explicit
  // fields/terms, but still collect index-level statistics
  // i.e. all fields and terms implicitly match
  bstring stats(order.stats_size(), 0);
  auto* stats_buf = const_cast<byte_type*>(stats.data());

  order.prepare_stats(stats_buf);
  order.prepare_collectors(stats_buf, reader);

  filter_boost *= boost();

  return prefix_match_
    ? filter::prepared::make<column_prefix_existence_query>(field_, std::move(stats), filter_boost)
    : filter::prepared::make<column_existence_query>(field_, std::move(stats), filter_boost);
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

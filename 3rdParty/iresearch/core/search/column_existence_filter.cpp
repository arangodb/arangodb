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
////////////////////////////////////////////////////////////////////////////////

#include "column_existence_filter.hpp"

#include "formats/empty_term_reader.hpp"
#include "search/disjunction.hpp"

namespace {

using namespace irs;

class column_existence_query : public irs::filter::prepared {
 public:
  explicit column_existence_query(
      const std::string& field,
      bstring&& stats,
      boost_t boost)
    : filter::prepared(boost),
      field_(field),
      stats_(std::move(stats)) {
  }

  virtual doc_iterator::ptr execute(
      const sub_reader& segment,
      const order::prepared& ord,
      const attribute_provider* /*ctx*/) const override {
    const auto* column = segment.column_reader(field_);

    if (!column) {
      return doc_iterator::empty();
    }

    return iterator(segment, *column, ord);
  }

 protected:
  doc_iterator::ptr iterator(
      const sub_reader& segment,
      const columnstore_reader::column_reader& column,
      const order::prepared& ord) const {
    auto it = column.iterator();

    if (IRS_UNLIKELY(!it)) {
      return doc_iterator::empty();
    }

    if (!ord.empty()) {
      auto* score = irs::get_mutable<irs::score>(it.get());

      if (score) {
        order::prepared::scorers scorers(
          ord, segment, empty_term_reader(column.size()),
          stats_.c_str(), score->realloc(ord), *it, boost());

        irs::reset(*score, std::move(scorers));
      }
    }

    return it;
  }

  std::string field_;
  bstring stats_;
}; // column_existence_query

class column_prefix_existence_query final : public column_existence_query {
 public:
  explicit column_prefix_existence_query(
      const std::string& prefix,
      bstring&& stats,
      boost_t boost)
    : column_existence_query(prefix, std::move(stats), boost) {
  }

  virtual irs::doc_iterator::ptr execute(
      const irs::sub_reader& segment,
      const irs::order::prepared& ord,
      const irs::attribute_provider* /*ctx*/) const override {
    using scored_disjunction_t = irs::scored_disjunction_iterator<irs::doc_iterator::ptr>;
    using disjunction_t = irs::disjunction_iterator<irs::doc_iterator::ptr>;

    const string_ref prefix = field_;

    auto it = segment.columns();

    if (!it->seek(prefix)) {
      // reached the end
      return irs::doc_iterator::empty();
    }

    disjunction_t::doc_iterators_t itrs;

    while (irs::starts_with(it->value().name, prefix)) {
      const auto* column = segment.column_reader(it->value().id);

      if (!column) {
        continue;
      }

      itrs.emplace_back(iterator(segment, *column, ord));

      if (!it->next()) {
        break;
      }
    }

    if (ord.empty()) {
      return irs::make_disjunction<disjunction_t>(std::move(itrs));
    }

    return irs::make_disjunction<scored_disjunction_t>(std::move(itrs), ord);
  }
}; // column_prefix_existence_query

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                by_column_existence implementation
// -----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(by_column_existence)

filter::prepared::ptr by_column_existence::prepare(
    const index_reader& reader,
    const order::prepared& order,
    boost_t filter_boost,
    const attribute_provider* /*ctx*/) const {
  // skip field-level/term-level statistics because there are no explicit
  // fields/terms, but still collect index-level statistics
  // i.e. all fields and terms implicitly match
  bstring stats(order.stats_size(), 0);
  auto* stats_buf = const_cast<byte_type*>(stats.data());

  order.prepare_collectors(stats_buf, reader);

  filter_boost *= boost();

  return options().prefix_match
    ? memory::make_managed<column_prefix_existence_query>(field(), std::move(stats), filter_boost)
    : memory::make_managed<column_existence_query>(field(), std::move(stats), filter_boost);
}

} // ROOT

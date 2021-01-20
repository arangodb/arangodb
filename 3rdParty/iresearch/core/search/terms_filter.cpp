////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "terms_filter.hpp"

#include "index/index_reader.hpp"
#include "search/all_terms_collector.hpp"
#include "search/collectors.hpp"
#include "search/term_filter.hpp"
#include "search/filter_visitor.hpp"
#include "search/multiterm_query.hpp"

namespace {

using namespace irs;

template<typename Visitor>
void visit(
    const sub_reader& segment,
    const term_reader& field,
    const by_terms_options::search_terms& search_terms,
    Visitor& visitor) {
  auto terms = field.iterator();

  if (IRS_UNLIKELY(!terms)) {
    return;
  }

  visitor.prepare(segment, field, *terms);

  for (auto& term : search_terms) {
    if (!terms->seek(term.term)) {
      continue;
    }

    terms->read();

    visitor.visit(term.boost);
  }
}

template<typename Collector>
class terms_visitor {
 public:
  explicit terms_visitor(Collector& collector) noexcept
    : collector_(collector) {
  }

  void prepare(
      const sub_reader& segment,
      const term_reader& field,
      const seek_term_iterator& terms) {
    collector_.prepare(segment, field, terms);
    collector_.stat_index(0);
  }

  void visit(boost_t boost) {
    size_t stat_index = collector_.stat_index();
    collector_.visit(boost);
    collector_.stat_index(++stat_index);
  }

 private:
  Collector& collector_;
}; // terms_visitor

template<typename Collector>
void collect_terms(
    const index_reader& index,
    const string_ref& field,
    const by_terms_options::search_terms& terms,
    Collector& collector) {
  terms_visitor<Collector> visitor(collector);

  for (auto& segment : index) {
    auto* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    visit(segment, *reader, terms, visitor);
  }
}

}

namespace iresearch {

DEFINE_FACTORY_DEFAULT(by_terms)

/*static*/ void by_terms::visit(
    const sub_reader& segment,
    const term_reader& field,
    const by_terms_options::search_terms& terms,
    filter_visitor& visitor) {
  ::visit(segment, field, terms, visitor);
}

filter::prepared::ptr by_terms::prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const attribute_provider* /*ctx*/) const {
  boost *= this->boost();
  const auto& terms = options().terms;
  const size_t size = terms.size();

  if (0 == size) {
    return prepared::empty();
  }

  if (1 == size) {
    const auto term = terms.begin();
    return by_term::prepare(index, order, boost*term->boost, field(), term->term);
  }

  field_collectors field_stats(order);
  term_collectors term_stats(order, size);
  multiterm_query::states_t states(index.size());

  all_terms_collector<decltype(states)> collector(states, field_stats, term_stats);
  collect_terms(index, field(), terms, collector);

  std::vector<bstring> stats(size);
  size_t term_idx = 0;
  for (auto& stat : stats) {
    stat.resize(order.stats_size(), 0);
    auto* stats_buf = const_cast<byte_type*>(stat.data());
    term_stats.finish(stats_buf, term_idx++, field_stats, index);
  }

  return memory::make_managed<multiterm_query>(
    std::move(states), std::move(stats),
    boost, sort::MergeType::AGGREGATE);
}

}

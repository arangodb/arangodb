////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "shared.hpp"
#include "wildcard_filter.hpp"
#include "limited_sample_scorer.hpp"
#include "all_filter.hpp"
#include "disjunction.hpp"
#include "multiterm_query.hpp"
#include "term_query.hpp"
#include "bitset_doc_iterator.hpp"
#include "index/index_reader.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/hash_utils.hpp"

NS_LOCAL

using wildcard_traits_t = irs::wildcard_traits<irs::byte_type>;

NS_END

NS_ROOT

WildcardType wildcard_type(const irs::bstring& expr) noexcept {
  if (expr.empty()) {
    return WildcardType::TERM;
  }

  bool escaped = false;
  size_t num_match_any_string = 0;
  for (const auto c : expr) {
    switch (c) {
      case wildcard_traits_t::MATCH_ANY_STRING:
        num_match_any_string = size_t(!escaped);
        break;
      case wildcard_traits_t::MATCH_ANY_CHAR:
        if (!escaped) {
          return WildcardType::WILDCARD;
        }
        break;
      case wildcard_traits_t::ESCAPE:
       escaped = !escaped;
       break;
    }
  }

  if (0 == num_match_any_string) {
    return WildcardType::TERM;
  }

  if (1 == num_match_any_string) {
    if (1 == expr.size()) {
      return WildcardType::MATCH_ALL;
    }

    if (wildcard_traits_t::MATCH_ANY_STRING == expr.back()) {
      return WildcardType::PREFIX;
    }
  }

  return WildcardType::WILDCARD;
}

DEFINE_FILTER_TYPE(by_wildcard)
DEFINE_FACTORY_DEFAULT(by_wildcard)

filter::prepared::ptr by_wildcard::prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const attribute_view& /*ctx*/) const {
  boost *= this->boost();

  const string_ref field = this->field();
  const auto wildcard_type = irs::wildcard_type(term());

  switch (wildcard_type) {
    case WildcardType::TERM:
      return term_query::make(index, order, boost, field, term());
    case WildcardType::MATCH_ALL:
      return by_prefix::prepare(index, order, boost, field,
                                bytes_ref::EMPTY, // empty prefix == match all
                                scored_terms_limit());
    case WildcardType::PREFIX:
      assert(!term().empty());
      return by_prefix::prepare(index, order, boost, field,
                                bytes_ref(term().c_str(), term().size() - 1), // remove trailing '%'
                                scored_terms_limit());
    default:
      break;
  }

  assert(WildcardType::WILDCARD == wildcard_type);

  limited_sample_scorer scorer(order.empty() ? 0 : scored_terms_limit()); // object for collecting order stats
  multiterm_query::states_t states(index.size());
  auto acceptor = from_wildcard<byte_type, wildcard_traits_t>(term());

  for (const auto& segment : index) {
    // get term dictionary for field
    const term_reader* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    auto it = reader->iterator(acceptor);

    auto& meta = it->attributes().get<term_meta>(); // get term metadata
    const decltype(irs::term_meta::docs_count) NO_DOCS = 0;
    const auto& docs_count = meta ? meta->docs_count : NO_DOCS;

    if (it->next()) {
      auto& state = states.insert(segment);
      state.reader = reader;

      do {
        it->read(); // read term attributes

        state.estimation += docs_count;
        scorer.collect(docs_count, state.count++, state, segment, *it);
      } while (it->next());
    }
  }

  std::vector<bstring> stats;
  scorer.score(index, order, stats);

  return memory::make_shared<multiterm_query>(std::move(states), std::move(stats), boost);
}

by_wildcard::by_wildcard() noexcept
  : by_prefix(by_wildcard::type()) {
}

NS_END

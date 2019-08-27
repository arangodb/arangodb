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
#include "all_filter.hpp"
#include "range_query.hpp"
#include "term_query.hpp"
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

struct multiterm_state {
  const term_reader* reader;
  cost::cost_t estimation;
  std::vector<seek_term_iterator::seek_cookie::ptr> cookies;
};

//////////////////////////////////////////////////////////////////////////////
/// @class multiterm_query
/// @brief compiled query suitable for filters with non adjacent set of terms
//////////////////////////////////////////////////////////////////////////////
class multiterm_query : public filter::prepared {
 public:
  typedef states_cache<multiterm_state> states_t;

  DECLARE_SHARED_PTR(multiterm_query);

  explicit multiterm_query(states_t&& states, boost_t boost)
    : prepared(boost), states_(std::move(states)) {
  }

  virtual doc_iterator::ptr execute(
      const sub_reader& rdr,
      const order::prepared& ord,
      const attribute_view& /*ctx*/) const override;

 private:
  states_t states_;
}; // multiterm_query

doc_iterator::ptr multiterm_query::execute(
    const sub_reader& segment,
    const order::prepared& ord,
    const attribute_view& /*ctx*/) const {
  auto state = states_.find(segment);

  if (!state) {
    // invalid state
    return doc_iterator::empty();
  }



  return doc_iterator::empty();
}

DEFINE_FILTER_TYPE(by_wildcard)
DEFINE_FACTORY_DEFAULT(by_wildcard)

filter::prepared::ptr by_wildcard::prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& ctx) const {
  const string_ref field = this->field();
  const auto wildcard_type = irs::wildcard_type(term());

  switch (wildcard_type) {
    case WildcardType::MATCH_ALL:
      return all().prepare(index, ord, this->boost()*boost, ctx);
    case WildcardType::TERM:
      return term_query::make(index, ord, this->boost()*boost, field, term());
    case WildcardType::PREFIX:
      return by_prefix::prepare(index, ord, boost, ctx);
    default:
      break;
  }

  assert(WildcardType::WILDCARD == wildcard_type);

  multiterm_query::states_t states(index.size());
  limited_sample_scorer scorer(ord.empty() ? 0 : scored_terms_limit()); // object for collecting order stats
  auto acceptor = from_wildcard<byte_type, wildcard_traits_t>(term());

  for (const auto& segment : index) {
    // get term dictionary for field
    const term_reader* reader = segment.field(field);

    if (!reader) {
      continue;
    }

    intersect_term_iterator it(acceptor, reader->iterator());

    auto& meta = it.attributes().get<term_meta>(); // get term metadata
    const decltype(irs::term_meta::docs_count) NO_DOCS = 0;
    const auto& docs_count = meta ? meta->docs_count : NO_DOCS;

    if (it.next()) {
      auto& state = states.insert(segment);
      state.reader = reader;

      do {
        // read term attributes
        it.read();

        // get state for current term
        state.cookies.emplace_back(it.cookie());
        state.estimation += docs_count;
      } while (it.next());
    }
  }

  return std::make_shared<multiterm_query>(std::move(states), this->boost()*boost);
}

by_wildcard::by_wildcard() noexcept
  : by_prefix(by_wildcard::type()) {
}

NS_END

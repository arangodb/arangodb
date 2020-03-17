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

#include "wildcard_filter.hpp"

#include "shared.hpp"
#include "limited_sample_scorer.hpp"
#include "multiterm_query.hpp"
#include "term_query.hpp"
#include "index/index_reader.hpp"
#include "utils/wildcard_utils.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/hash_utils.hpp"

NS_LOCAL

inline irs::bytes_ref unescape(const irs::bytes_ref& in, irs::bstring& out) {
  out.reserve(in.size());

  bool copy = true;
  std::copy_if(in.begin(), in.end(), std::back_inserter(out),
               [&copy](irs::byte_type c) {
    if (c == irs::WildcardMatch::ESCAPE) {
      copy = !copy;
    } else {
      copy = true;
    }
    return copy;
  });

  return out;
}

NS_END

NS_ROOT

DEFINE_FILTER_TYPE(by_wildcard)
DEFINE_FACTORY_DEFAULT(by_wildcard)

/*static*/ filter::prepared::ptr by_wildcard::prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const string_ref& field,
    bytes_ref term,
    size_t scored_terms_limit) {
  bstring buf;
  switch (wildcard_type(term)) {
    case WildcardType::INVALID:
      return prepared::empty();
    case WildcardType::TERM_ESCAPED:
      term = unescape(term, buf);
      [[fallthrough]];
    case WildcardType::TERM:
      return term_query::make(index, order, boost, field, term);
    case WildcardType::MATCH_ALL:
      return by_prefix::prepare(index, order, boost, field,
                                bytes_ref::EMPTY, // empty prefix == match all
                                scored_terms_limit);
    case WildcardType::PREFIX_ESCAPED:
      term = unescape(term, buf);
      [[fallthrough]];
    case WildcardType::PREFIX: {
      assert(!term.empty());
      const auto* begin = term.c_str();
      const auto* end = begin + term.size();

      // term is already checked to be a valid UTF-8 sequence
      const auto* pos = utf8_utils::find<false>(begin, end, WildcardMatch::ANY_STRING);
      assert(pos != end);

      return by_prefix::prepare(index, order, boost, field,
                                bytes_ref(begin, size_t(pos - begin)), // remove trailing '%'
                                scored_terms_limit);
    }

    case WildcardType::WILDCARD:
      return prepare_automaton_filter(field, from_wildcard(term),
                                      scored_terms_limit, index, order, boost);
  }

  assert(false);
  return prepared::empty();
}

by_wildcard::by_wildcard() noexcept
  : by_prefix(by_wildcard::type()) {
}

NS_END

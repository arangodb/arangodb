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
#include "search/filter_visitor.hpp"
#include "search/multiterm_query.hpp"
#include "search/term_filter.hpp"
#include "search/prefix_filter.hpp"
#include "index/index_reader.hpp"
#include "utils/wildcard_utils.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/hash_utils.hpp"

namespace {

using namespace irs;

inline bytes_ref unescape(const bytes_ref& in, bstring& out) {
  out.reserve(in.size());

  bool copy = true;
  std::copy_if(in.begin(), in.end(), std::back_inserter(out),
               [&copy](byte_type c) {
    if (c == WildcardMatch::ESCAPE) {
      copy = !copy;
    } else {
      copy = true;
    }
    return copy;
  });

  return out;
}

template<typename Invalid, typename Term, typename Prefix, typename WildCard>
inline auto executeWildcard(
    bstring& buf, bytes_ref term,
    Invalid inv, Term t, Prefix p, WildCard w) {
  switch (wildcard_type(term)) {
    case WildcardType::INVALID:
      return inv();
    case WildcardType::TERM_ESCAPED:
      term = unescape(term, buf);
    [[fallthrough]];
    case WildcardType::TERM:
      return t(term);
    case WildcardType::MATCH_ALL:
      term = bytes_ref::EMPTY;
      return p(term);
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

      term = bytes_ref(begin, size_t(pos - begin)); // remove trailing '%'
      return p(term);
    }
    case WildcardType::WILDCARD:
      return w(term);
    default:
      assert(false);
      return inv();
  }
}

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                        by_wildcard implementation
// -----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(by_wildcard)

field_visitor by_wildcard::visitor(const bytes_ref& term) {
  bstring buf;
  return executeWildcard(
    buf, term,
    []() -> field_visitor {
      return [](const sub_reader&, const term_reader&, filter_visitor&) { };
    },
    [](const bytes_ref& term) -> field_visitor {
      // must copy term as it may point to temporary string
      return [term = bstring(term)](
          const sub_reader& segment,
          const term_reader& field,
          filter_visitor& visitor) {
        by_term::visit(segment, field, term, visitor);
      };
    },
    [](const bytes_ref& term) -> field_visitor {
      // must copy term as it may point to temporary string
      return [term = bstring(term)](
          const sub_reader& segment,
          const term_reader& field,
          filter_visitor& visitor) {
        by_prefix::visit(segment, field, term, visitor);
      };
    },
    [](const bytes_ref& term) -> field_visitor{
      struct automaton_context : util::noncopyable {
        automaton_context(const bytes_ref& term)
          : acceptor(from_wildcard(term)),
            matcher(make_automaton_matcher(acceptor)) {
        }

        automaton acceptor;
        automaton_table_matcher matcher;
      };

      // FIXME
      auto ctx = memory::make_shared<automaton_context>(term);

      if (!validate(ctx->acceptor)) {
        return [](const sub_reader&, const term_reader&, filter_visitor&) { };
      }

      return [ctx](
          const sub_reader& segment,
          const term_reader& field,
          filter_visitor& visitor) mutable {
        return irs::visit(segment, field, ctx->matcher, visitor);
      };
    }
  );
}

/*static*/ filter::prepared::ptr by_wildcard::prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& term,
    size_t scored_terms_limit) {
  bstring buf;
  return executeWildcard(
    buf, term,
    []() -> filter::prepared::ptr {
      return prepared::empty();
    },
    [&index, &order, boost, &field](const bytes_ref& term) -> filter::prepared::ptr {
      return by_term::prepare(index, order, boost, field, term);
    },
    [&index, &order, boost, &field, scored_terms_limit](const bytes_ref& term) -> filter::prepared::ptr {
      return by_prefix::prepare(index, order, boost, field, term, scored_terms_limit);
    },
    [&index, &order, boost, &field, scored_terms_limit](const bytes_ref& term) -> filter::prepared::ptr {
      return prepare_automaton_filter(field, from_wildcard(term), scored_terms_limit,
                                      index, order, boost);
    }
  );
}

}

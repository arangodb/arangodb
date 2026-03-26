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

#include "index/index_reader.hpp"
#include "search/filter_visitor.hpp"
#include "search/multiterm_query.hpp"
#include "search/prefix_filter.hpp"
#include "search/term_filter.hpp"
#include "shared.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/hash_utils.hpp"
#include "utils/wildcard_utils.hpp"

namespace irs {
namespace {

bytes_view Unescape(bytes_view in, bstring& out) {
  out.reserve(in.size());

  bool copy = true;
  std::copy_if(in.begin(), in.end(), std::back_inserter(out),
               [&copy](byte_type c) {
                 if (c == WildcardMatch::kEscape) {
                   copy = !copy;
                 } else {
                   copy = true;
                 }
                 return copy;
               });

  return out;
}

template<typename Term, typename Prefix, typename WildCard>
auto ExecuteWildcard(bstring& buf, bytes_view term, Term&& t, Prefix&& p,
                     WildCard&& w) {
  switch (ComputeWildcardType(term)) {
    case WildcardType::kTermEscaped:
      term = Unescape(term, buf);
      [[fallthrough]];
    case WildcardType::kTerm:
      return t(term);
    case WildcardType::kPrefixEscaped:
      term = Unescape(term, buf);
      [[fallthrough]];
    case WildcardType::kPrefix: {
      IRS_ASSERT(!term.empty());
      const auto idx = term.find_first_of(WildcardMatch::kAnyStr);
      IRS_ASSERT(idx != bytes_view::npos);
      term = bytes_view{term.data(), idx};  // remove trailing '%'
      return p(term);
    }
    case WildcardType::kWildcard:
      return w(term);
  }
}

}  // namespace

field_visitor by_wildcard::visitor(bytes_view term) {
  bstring buf;
  return ExecuteWildcard(
    buf, term,
    [](bytes_view term) -> field_visitor {
      // must copy term as it may point to temporary string
      return [term = bstring(term)](const SubReader& segment,
                                    const term_reader& field,
                                    filter_visitor& visitor) {
        by_term::visit(segment, field, term, visitor);
      };
    },
    [](bytes_view term) -> field_visitor {
      // must copy term as it may point to temporary string
      return [term = bstring(term)](const SubReader& segment,
                                    const term_reader& field,
                                    filter_visitor& visitor) {
        by_prefix::visit(segment, field, term, visitor);
      };
    },
    [](bytes_view term) -> field_visitor {
      struct AutomatonContext : util::noncopyable {
        explicit AutomatonContext(bytes_view term)
          : acceptor{FromWildcard(term)},
            matcher{MakeAutomatonMatcher(acceptor)} {}

        automaton acceptor;
        automaton_table_matcher matcher;
      };

      auto ctx = std::make_shared<AutomatonContext>(term);

      if (!Validate(ctx->acceptor)) {
        return [](const SubReader&, const term_reader&, filter_visitor&) {};
      }

      return [ctx = std::move(ctx)](const SubReader& segment,
                                    const term_reader& field,
                                    filter_visitor& visitor) mutable {
        return irs::Visit(segment, field, ctx->matcher, visitor);
      };
    });
}

filter::prepared::ptr by_wildcard::prepare(const PrepareContext& ctx,
                                           std::string_view field,
                                           bytes_view term,
                                           size_t scored_terms_limit) {
  bstring buf;
  return ExecuteWildcard(
    buf, term,
    [&](bytes_view term) -> prepared::ptr {
      return by_term::prepare(ctx, field, term);
    },
    [&, scored_terms_limit](bytes_view term) -> prepared::ptr {
      return by_prefix::prepare(ctx, field, term, scored_terms_limit);
    },
    [&, scored_terms_limit](bytes_view term) -> prepared::ptr {
      return PrepareAutomatonFilter(ctx, field, FromWildcard(term),
                                    scored_terms_limit);
    });
}

}  // namespace irs

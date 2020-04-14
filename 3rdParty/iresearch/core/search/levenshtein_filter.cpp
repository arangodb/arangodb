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

#include "levenshtein_filter.hpp"

#include "shared.hpp"
#include "term_query.hpp"
#include "index/index_reader.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/levenshtein_utils.hpp"
#include "utils/levenshtein_default_pdp.hpp"
#include "utils/hash_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/std.hpp"

NS_LOCAL

using namespace irs;

template<typename Invalid, typename Term, typename Levenshtein>
inline void executeLevenshtein(byte_type max_distance,
                               by_edit_distance::pdp_f provider, bool with_transpositions,
                               Invalid inv, Term t, Levenshtein lev) {
  if (0 == max_distance) {
    t();
    return;
  }

  assert(provider);
  const auto& d = (*provider)(max_distance, with_transpositions);

  if (!d) {
    inv();
    return;
  }

  lev(d);
}

NS_END

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                   by_edit_distance implementation
// -----------------------------------------------------------------------------

DEFINE_FILTER_TYPE(by_edit_distance)
DEFINE_FACTORY_DEFAULT(by_edit_distance)

/*static*/ filter::prepared::ptr by_edit_distance::prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& term,
    size_t scored_terms_limit,
    byte_type max_distance,
    pdp_f provider,
    bool with_transpositions) {
  filter::prepared::ptr res;
  executeLevenshtein(
    max_distance, provider, with_transpositions,
    [&res]() {
      res = prepared::empty();},
    [&res, &index, &order, boost, &field, &term]() {
      res = term_query::make(index, order, boost, field, term);},
    [&res, &field, &term, scored_terms_limit, &index, &order, boost](const parametric_description& d) {
      res = prepare_automaton_filter(field, make_levenshtein_automaton(d, term),
                                     scored_terms_limit, index, order, boost);}
  );
  return res;
}

/*static*/ void by_edit_distance::visit(
    const term_reader& reader,
    const bytes_ref& term,
    byte_type max_distance,
    by_edit_distance::pdp_f provider,
    bool with_transpositions,
    filter_visitor& fv) {
  executeLevenshtein(
    max_distance, provider, with_transpositions,
    []() {},
    [&reader, &term, &fv]() {
      term_query::visit(reader, term, fv);
    },
    [&reader, &term, &fv](const parametric_description& d) {
      automaton_visit(reader, make_levenshtein_automaton(d, term), fv);
    }
  );
}

by_edit_distance::by_edit_distance() noexcept
  : by_prefix(by_edit_distance::type()),
    provider_(&default_pdp) {
}

by_edit_distance& by_edit_distance::provider(pdp_f provider) noexcept {
  if (!provider) {
    provider_ = &default_pdp;
  } else {
    provider_ = provider;
  }
  return *this;
}

size_t by_edit_distance::hash() const noexcept {
  size_t seed = 0;
  seed = hash_combine(0, by_prefix::hash());
  seed = hash_combine(seed, max_distance_);
  seed = hash_combine(seed, with_transpositions_);
  return seed;
}

bool by_edit_distance::equals(const filter& rhs) const noexcept {
  const auto& impl = static_cast<const by_edit_distance&>(rhs);

  return by_prefix::equals(rhs) &&
    max_distance_ == impl.max_distance_ &&
    with_transpositions_ == impl.with_transpositions_;
}

NS_END

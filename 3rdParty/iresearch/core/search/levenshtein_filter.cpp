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
#include "limited_sample_scorer.hpp"
#include "index/index_reader.hpp"
#include "utils/automaton_utils.hpp"
#include "utils/levenshtein_utils.hpp"
#include "utils/hash_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/std.hpp"

NS_ROOT

const irs::parametric_description& parametric_description_provider(
    irs::byte_type distance,
    bool with_transpositions) {
  struct builder {
    using type = irs::parametric_description;

    static type make(size_t idx) {
      const auto max_distance = irs::byte_type(idx >> 1);
      const auto with_transpositions = 0 != (idx % 2);
      return irs::make_parametric_description(max_distance, with_transpositions);
    }
  };

  const size_t idx = 2*size_t(distance) + size_t(with_transpositions);
  return irs::irstd::static_lazy_array<builder, 9>::at(idx);
}

DEFINE_FILTER_TYPE(by_edit_distance)
DEFINE_FACTORY_DEFAULT(by_edit_distance)

filter::prepared::ptr by_edit_distance::prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const attribute_view& ctx) const {
  if (0 == max_distance_) {
    return by_term::prepare(index, order, boost, ctx);
  }

  assert(provider_);
  const auto& d = (*provider_)(max_distance_, with_transpositions_);

  if (!d) {
    assert(false);
    return prepared::empty();
  }

  boost *= this->boost();
  const string_ref field = this->field();

  return prepare_automaton_filter(field, make_levenshtein_automaton(d, term()),
                                  scored_terms_limit(), index, order, boost);
}

by_edit_distance::by_edit_distance() noexcept
  : by_prefix(by_edit_distance::type()) {
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

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_GRANULAR_RANGE_FILTER_H
#define IRESEARCH_GRANULAR_RANGE_FILTER_H

#include "search/filter.hpp"
#include "search/search_range.hpp"
#include "utils/string.hpp"

namespace iresearch {

class by_granular_range;
class numeric_token_stream;
struct filter_visitor;

////////////////////////////////////////////////////////////////////////////////
/// @struct by_granular_range_options
/// @brief options for granular range filter
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API by_granular_range_options {
  using filter_type = by_granular_range;

  using terms = std::vector<bstring>;
  using range_type = search_range<terms>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief search range
  /// @note terms are expected to be placed by granularity levels from the most
  ///       precise term to the less precise one, i.e. lower indexes denote more
  ///       precise term
  /// @note consider using "set_granular_term" function for convenience
  //////////////////////////////////////////////////////////////////////////////
  range_type range;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  size_t scored_terms_limit{1024};

  bool operator==(const by_granular_range_options& rhs) const noexcept {
    return range == rhs.range && scored_terms_limit == rhs.scored_terms_limit;
  }

  size_t hash() const noexcept {
    return hash_combine(scored_terms_limit, range);
  }
}; // by_granular_range_options

//////////////////////////////////////////////////////////////////////////////
/// @brief convenient helper for setting granular term at a specified range
///        boundary
/// @note use the most precise value of 'granularity_level'
//////////////////////////////////////////////////////////////////////////////
template<typename T>
void set_granular_term(by_granular_range_options::terms& boundary, T&& value) {
  boundary.clear();
  boundary.emplace_back(std::forward<T>(value));
}

//////////////////////////////////////////////////////////////////////////////
/// @brief convenient helper for setting granular term at a specified range
///        boundary
//////////////////////////////////////////////////////////////////////////////
IRESEARCH_API void set_granular_term(
  by_granular_range_options::terms& boundary,
  numeric_token_stream& term);

//////////////////////////////////////////////////////////////////////////////
/// @class by_granular_range
/// @brief user-side term range filter for granularity-enabled terms
///        when indexing, the lower the value for attributes().get<position>()
///        the higher the granularity of the term value
///        the lower granularity terms are <= higher granularity terms
///        NOTE: it is assumed that granularity level gaps are identical for
///              all terms, i.e. the behavour for the following is undefined:
///              termA@0 + termA@2 + termA@5 + termA@10
///              termB@0 + termB@2 + termB@6 + termB@10
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_granular_range
    : public filter_base<by_granular_range_options> {
 public:
  DECLARE_FACTORY();

  static filter::prepared::ptr prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const options_type::range_type& rng,
    size_t scored_terms_limit);

  static void visit(
    const sub_reader& segment,
    const term_reader& reader,
    const options_type::range_type& rng,
    filter_visitor& visitor);

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
      const index_reader& index,
      const order::prepared& ord,
      boost_t boost,
      const attribute_provider* /*ctx*/) const override {
    return prepare(index, ord, this->boost()*boost,
                   field(), options().range,
                   options().scored_terms_limit);
  }
}; // by_granular_range

} // ROOT

#endif

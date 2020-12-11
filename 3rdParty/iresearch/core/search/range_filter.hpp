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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_RANGE_FILTER_H
#define IRESEARCH_RANGE_FILTER_H

#include "search/filter.hpp"
#include "search/search_range.hpp"
#include "utils/string.hpp"

namespace iresearch {

class by_range;
struct filter_visitor;

struct IRESEARCH_API by_range_filter_options {
  using range_type = search_range<bstring>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief search range
  //////////////////////////////////////////////////////////////////////////////
  range_type range;

  bool operator==(const by_range_filter_options& rhs) const noexcept {
    return range == rhs.range;
  }

  size_t hash() const noexcept {
    return std::hash<range_type>()(range);
  }
}; // by_range_filter_options

////////////////////////////////////////////////////////////////////////////////
/// @struct by_prefix_options
/// @brief options for prefix filter
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API by_range_options : by_range_filter_options {
  using filter_type = by_range;
  using filter_options = by_range_filter_options;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  size_t scored_terms_limit{1024};

  bool operator==(const by_range_options& rhs) const noexcept {
    return filter_options::operator==(rhs) &&
      scored_terms_limit == rhs.scored_terms_limit;
  }

  size_t hash() const noexcept {
    return hash_combine(filter_options::hash(),
                        std::hash<decltype(scored_terms_limit)>()(scored_terms_limit));
  }
}; // by_range_options

//////////////////////////////////////////////////////////////////////////////
/// @class by_range
/// @brief user-side term range filter
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_range : public filter_base<by_range_options> {
 public:
  DECLARE_FACTORY();

  static prepared::ptr prepare(
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
}; // by_range 

} // ROOT

namespace std {

template<>
struct hash<::iresearch::by_range_options> {
  size_t operator()(const ::iresearch::by_range_options& v) const noexcept {
    return v.hash();
  }
};

}

#endif // IRESEARCH_RANGE_FILTER_H

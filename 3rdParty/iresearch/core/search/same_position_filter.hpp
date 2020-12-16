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

#ifndef IRESEARCH_SAME_POSITION_FILTER_H
#define IRESEARCH_SAME_POSITION_FILTER_H

#include "filter.hpp"
#include "utils/string.hpp"

namespace iresearch {

class by_same_position;

////////////////////////////////////////////////////////////////////////////////
/// @struct by_terms_options
/// @brief options for "by same position" filter
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API by_same_position_options {
  using filter_type = by_same_position;

  using search_term = std::pair<std::string, bstring>;
  using search_terms = std::vector<search_term>;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief search terms
  //////////////////////////////////////////////////////////////////////////////
  search_terms terms;

  bool operator==(const by_same_position_options& rhs) const noexcept {
    return terms == rhs.terms;
  }

  size_t hash() const noexcept {
    size_t hash = 0;
    for (auto& term : terms) {
      hash = hash_combine(hash, term.first);
      hash = hash_combine(hash, term.second);
    }
    return hash;
  }
}; // by_same_position_options

//////////////////////////////////////////////////////////////////////////////
/// @class by_same_position
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_same_position
    : public filter_with_options<by_same_position_options> {
 public:
  DECLARE_FACTORY();

  //////////////////////////////////////////////////////////////////////////////
  /// @returns features required for filter
  //////////////////////////////////////////////////////////////////////////////
  static const flags& required();

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* ctx) const override;
}; // by_same_position

} // ROOT

#endif // IRESEARCH_SAME_POSITION_FILTER_H

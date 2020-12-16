////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_COLUMN_EXISTENCE_FILTER_H
#define IRESEARCH_COLUMN_EXISTENCE_FILTER_H

#include "filter.hpp"
#include "utils/string.hpp"

namespace iresearch {

class by_column_existence;

////////////////////////////////////////////////////////////////////////////////
/// @struct by_column_existence_options
/// @brief options for column existence filter
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API by_column_existence_options {
  using filter_type = by_column_existence;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief match field prefix
  //////////////////////////////////////////////////////////////////////////////
  bool prefix_match{};

  bool operator==(const by_column_existence_options& rhs) const noexcept {
    return prefix_match == rhs.prefix_match;
  }

  size_t hash() const noexcept {
    return std::hash<bool>()(prefix_match);
  }
}; // by_column_existence_options

//////////////////////////////////////////////////////////////////////////////
/// @class by_column_existence
/// @brief user-side column existence filter
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_column_existence final
    : public filter_base<by_column_existence_options> {
 public:
  DECLARE_FACTORY();

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* ctx) const override;
}; // by_column_existence

} // ROOT

#endif // IRESEARCH_COLUMN_EXISTENCE_FILTER_H

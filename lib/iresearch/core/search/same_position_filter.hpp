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

#pragma once

#include "filter.hpp"
#include "utils/string.hpp"

namespace irs {

class by_same_position;

// Options for "by same position" filter
struct by_same_position_options {
  using filter_type = by_same_position;

  using search_term = std::pair<std::string, bstring>;
  using search_terms = std::vector<search_term>;

  search_terms terms;

  bool operator==(const by_same_position_options& rhs) const noexcept {
    return terms == rhs.terms;
  }
};

class by_same_position : public FilterWithOptions<by_same_position_options> {
 public:
  // Returns features required for the filter
  static constexpr IndexFeatures kRequiredFeatures =
    IndexFeatures::FREQ | IndexFeatures::POS;

  prepared::ptr prepare(const PrepareContext& ctx) const final;
};

}  // namespace irs

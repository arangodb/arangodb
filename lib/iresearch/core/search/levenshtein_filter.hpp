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

#pragma once

#include "filter.hpp"
#include "utils/string.hpp"

namespace irs {

class by_edit_distance;
class parametric_description;
struct filter_visitor;

struct by_edit_distance_all_options {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief parametric description provider
  //////////////////////////////////////////////////////////////////////////////
  using pdp_f = const parametric_description& (*)(uint8_t, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief target value
  //////////////////////////////////////////////////////////////////////////////
  bstring term;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief match this number of characters from the beginning of the
  ///        target regardless of edit distance
  //////////////////////////////////////////////////////////////////////////////
  bstring prefix;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns current parametric description provider, nullptr - use default
  /// @note since creation of parametric description is expensive operation,
  ///       especially for distances > 4, expert users may want to set its own
  ///       providers
  //////////////////////////////////////////////////////////////////////////////
  pdp_f provider{};

  //////////////////////////////////////////////////////////////////////////////
  /// @returns maximum allowed edit distance
  //////////////////////////////////////////////////////////////////////////////
  uint8_t max_distance{0};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief consider transpositions as an atomic change
  //////////////////////////////////////////////////////////////////////////////
  bool with_transpositions{false};
};

////////////////////////////////////////////////////////////////////////////////
/// @struct by_edit_distance_options
/// @brief options for levenshtein filter
////////////////////////////////////////////////////////////////////////////////
struct by_edit_distance_options : by_edit_distance_all_options {
  using filter_type = by_edit_distance;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum number of the most relevant terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  size_t max_terms{};

  bool operator==(const by_edit_distance_options& rhs) const noexcept {
    return term == rhs.term && max_distance == rhs.max_distance &&
           with_transpositions == rhs.with_transpositions &&
           max_terms == rhs.max_terms;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @class by_edit_distance
/// @brief user-side levenstein filter
////////////////////////////////////////////////////////////////////////////////
class by_edit_distance final
  : public FilterWithField<by_edit_distance_options> {
 public:
  static prepared::ptr prepare(const PrepareContext& ctx,
                               std::string_view field, bytes_view term,
                               size_t terms_limit, uint8_t max_distance,
                               options_type::pdp_f provider,
                               bool with_transpositions, bytes_view prefix);

  static field_visitor visitor(const by_edit_distance_all_options& options);

  prepared::ptr prepare(const PrepareContext& ctx) const final {
    auto sub_ctx = ctx;
    sub_ctx.boost *= boost();
    return prepare(sub_ctx, field(), options().term, options().max_terms,
                   options().max_distance, options().provider,
                   options().with_transpositions, options().prefix);
  }
};

}  // namespace irs

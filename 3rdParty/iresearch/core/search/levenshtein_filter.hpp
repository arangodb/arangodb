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

#ifndef IRESEARCH_LEVENSHTEIN_FILTER_H
#define IRESEARCH_LEVENSHTEIN_FILTER_H

#include "filter.hpp"
#include "prefix_filter.hpp"
#include "utils/string.hpp"

NS_ROOT

class parametric_description;

////////////////////////////////////////////////////////////////////////////////
/// @class by_edit_distance
/// @brief user-side levenstein filter
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_edit_distance final : public by_prefix {
 public:
  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parametric description provider
  //////////////////////////////////////////////////////////////////////////////
  using pdp_f = const parametric_description&(*)(byte_type, bool);

  explicit by_edit_distance() noexcept;

  using by_prefix::field;

  by_edit_distance& field(std::string fld) {
    by_prefix::field(std::move(fld));
    return *this;
  }

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& ctx
  ) const override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  by_edit_distance& scored_terms_limit(size_t limit) noexcept {
    by_prefix::scored_terms_limit(limit);
    return *this;
  }

  using by_prefix::scored_terms_limit;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets maximum allowed edit distance
  //////////////////////////////////////////////////////////////////////////////
  by_edit_distance& max_distance(byte_type limit) noexcept {
    max_distance_ = limit;
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns maximum allowed edit distance
  //////////////////////////////////////////////////////////////////////////////
  byte_type max_distance() const noexcept {
    return max_distance_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets option contollling if we need to consider transpositions
  ///        as an atomic change
  //////////////////////////////////////////////////////////////////////////////
  by_edit_distance& with_transpositions(bool with_transpositions) noexcept {
    with_transpositions_ = with_transpositions;
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns option controlling if we need to consider transpositions
  ///        as an atomic change
  //////////////////////////////////////////////////////////////////////////////
  bool with_transpositions() const noexcept {
    return with_transpositions_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns current parametric description provider
  //////////////////////////////////////////////////////////////////////////////
  pdp_f provider() const noexcept { return provider_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets parametric description provider,
  ///        nullptr == use default
  /// @note since creation of parametric description is expensive operation,
  ///       especially for distances > 4, expert users may want to set its own
  ///       providers
  //////////////////////////////////////////////////////////////////////////////
  by_edit_distance& provider(pdp_f provider) noexcept;

 protected:
  size_t hash() const noexcept override;
  bool equals(const filter& rhs) const noexcept override;

 private:
  pdp_f provider_;
  byte_type max_distance_{0};
  bool with_transpositions_{false};
}; // by_edit_distance

#endif // IRESEARCH_LEVENSHTEIN_FILTER_H

NS_END

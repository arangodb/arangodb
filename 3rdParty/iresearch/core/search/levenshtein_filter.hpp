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
#include "utils/string.hpp"

namespace iresearch {

class by_edit_distance;
class parametric_description;
struct filter_visitor;

struct IRESEARCH_API by_edit_distance_filter_options {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief parametric description provider
  //////////////////////////////////////////////////////////////////////////////
  using pdp_f = const parametric_description&(*)(byte_type, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief target value
  //////////////////////////////////////////////////////////////////////////////
  bstring term;

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
  byte_type max_distance{0};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief consider transpositions as an atomic change
  //////////////////////////////////////////////////////////////////////////////
  bool with_transpositions{false};

  bool operator==(const by_edit_distance_filter_options& rhs) const noexcept {
    return term == rhs.term &&
      max_distance == rhs.max_distance &&
      with_transpositions == rhs.with_transpositions;
  }

  size_t hash() const noexcept {
    return hash_combine(std::hash<bool>()(with_transpositions),
                        hash_combine(std::hash<bstring>()(term),
                                     std::hash<byte_type>()(max_distance)));
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @struct by_edit_distance_options
/// @brief options for levenshtein filter
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API by_edit_distance_options : by_edit_distance_filter_options {
  using filter_type = by_edit_distance;
  using filter_options = by_edit_distance_filter_options;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum number of the most relevant terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  size_t max_terms{};

  bool operator==(const by_edit_distance_options& rhs) const noexcept {
    return filter_options::operator==(rhs) &&
      max_terms == rhs.max_terms;
  }

  size_t hash() const noexcept {
    return hash_combine(filter_options::hash(), max_terms);
  }
}; // by_edit_distance_options

////////////////////////////////////////////////////////////////////////////////
/// @class by_edit_distance
/// @brief user-side levenstein filter
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_edit_distance final
    : public filter_base<by_edit_distance_options> {
 public:
  DECLARE_FACTORY();

  static prepared::ptr prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& term,
    size_t terms_limit,
    byte_type max_distance,
    options_type::pdp_f provider,
    bool with_transpositions);

  static field_visitor visitor(
    const options_type::filter_options& options);

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
      const index_reader& index,
      const order::prepared& order,
      boost_t boost,
      const attribute_provider* /*ctx*/) const override {
    return prepare(index, order, this->boost()*boost,
                   field(), options().term, options().max_terms,
                   options().max_distance, options().provider,
                   options().with_transpositions);
  }
}; // by_edit_distance

}

namespace std {

template<>
struct hash<::iresearch::by_edit_distance_filter_options> {
  size_t operator()(const ::iresearch::by_edit_distance_filter_options& v) const noexcept {
    return v.hash();
  }
};

}

#endif // IRESEARCH_LEVENSHTEIN_FILTER_H

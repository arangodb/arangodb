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

#ifndef IRESEARCH_PHRASE_FILTER_H
#define IRESEARCH_PHRASE_FILTER_H

#include <map>
#include <variant>

#include "search/levenshtein_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"
#include "search/terms_filter.hpp"
#include "search/wildcard_filter.hpp"
#include "utils/levenshtein_default_pdp.hpp"

namespace iresearch {

class by_phrase;

////////////////////////////////////////////////////////////////////////////////
/// @class by_phrase_options
/// @brief options for phrase filter
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_phrase_options {
 private:
  using phrase_part = std::variant<
    by_term_options,
    by_prefix_options,
    by_wildcard_options,
    by_edit_distance_filter_options,
    by_terms_options,
    by_range_options>;

  using phrase_type = std::map<size_t, phrase_part>;

 public:
  using filter_type = by_phrase;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief insert phrase part into the phrase at a specified position
  /// @returns reference to the inserted phrase part
  //////////////////////////////////////////////////////////////////////////////
  template<typename PhrasePart>
  PhrasePart& insert(size_t pos) {
    is_simple_term_only_ &= std::is_same<PhrasePart, by_term_options>::value; // constexpr

    return std::get<PhrasePart>(phrase_[pos]);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief insert phrase part into the phrase at a specified position
  /// @returns reference to the inserted phrase part
  //////////////////////////////////////////////////////////////////////////////
  template<typename PhrasePart>
  PhrasePart& insert(PhrasePart&& t, size_t pos) {
    is_simple_term_only_ &= std::is_same<PhrasePart, by_term_options>::value; // constexpr
    auto& part = (phrase_[pos] = std::forward<PhrasePart>(t));

    return std::get<std::decay_t<PhrasePart>>(part);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief appends phrase part of type "PhrasePart" at a specified offset
  ///        "offs" from the end of the phrase
  /// @returns reference to the inserted phrase part
  //////////////////////////////////////////////////////////////////////////////
  template<typename PhrasePart>
  PhrasePart& push_back(size_t offs = 0) {
    return insert(PhrasePart{}, next_pos() + offs);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief appends phrase part of type "PhrasePart" at a specified offset
  ///        "offs" from the end of the phrase
  /// @returns reference to the inserted phrase part
  //////////////////////////////////////////////////////////////////////////////
  template<typename PhrasePart>
  PhrasePart& push_back(PhrasePart&& t, size_t offs = 0) {
    return insert(std::forward<PhrasePart>(t), next_pos() + offs);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns pointer to the phrase part of type "PhrasePart" located at a
  ///          specified position, nullptr if actual type mismatches a requested
  ///          one
  //////////////////////////////////////////////////////////////////////////////
  template<typename PhrasePart>
  const PhrasePart* get(size_t pos) const noexcept {
    const auto it = phrase_.find(pos);

    if (it == phrase_.end()) {
      return nullptr;
    }

    return std::get_if<PhrasePart>(&it->second);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns true is options are equal, false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  bool operator==(const by_phrase_options& rhs) const noexcept {
    return phrase_ == rhs.phrase_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns hash value
  //////////////////////////////////////////////////////////////////////////////
  size_t hash() const noexcept {
    size_t hash = 0;
    for (auto& part : phrase_) {
      hash = hash_combine(hash, part.first);
      hash = hash_combine(hash, part.second);
    }
    return hash;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief clear phrase contents
  //////////////////////////////////////////////////////////////////////////////
  void clear() noexcept {
    phrase_.clear();
    is_simple_term_only_ = true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns true if phrase composed of simple terms only, false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  bool simple() const noexcept { return is_simple_term_only_; }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns true if phrase is empty, false - otherwise
  //////////////////////////////////////////////////////////////////////////////
  bool empty() const noexcept { return phrase_.empty(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns size of the phrase
  //////////////////////////////////////////////////////////////////////////////
  size_t size() const noexcept { return phrase_.size(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns iterator referring to the first part of the phrase
  //////////////////////////////////////////////////////////////////////////////
  phrase_type::const_iterator begin() const noexcept { return phrase_.begin(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns iterator referring to past-the-end element of the phrase
  //////////////////////////////////////////////////////////////////////////////
  phrase_type::const_iterator end() const noexcept { return phrase_.end(); }

 private:
  size_t next_pos() const {
    return phrase_.empty() ? 0 : 1 + phrase_.rbegin()->first;
  }

  phrase_type phrase_;
  bool is_simple_term_only_{true};
}; // by_phrase_options

////////////////////////////////////////////////////////////////////////////////
/// @class by_phrase
/// @brief user-side phrase filter
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_phrase : public filter_base<by_phrase_options> {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @returns features required for filter
  //////////////////////////////////////////////////////////////////////////////
  static const flags& required();

  DECLARE_FACTORY();

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const attribute_provider* ctx) const override;

 private:
  filter::prepared::ptr fixed_prepare_collect(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost) const;

  filter::prepared::ptr variadic_prepare_collect(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost) const;
}; // by_phrase

} // ROOT

#endif

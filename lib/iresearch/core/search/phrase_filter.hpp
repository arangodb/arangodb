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

#include <map>
#include <variant>

#include "search/levenshtein_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"
#include "search/terms_filter.hpp"
#include "search/wildcard_filter.hpp"
#include "utils/levenshtein_default_pdp.hpp"

namespace irs {

class by_phrase;

// Options for phrase filter
class by_phrase_options {
 private:
  using phrase_part =
    std::variant<by_term_options, by_prefix_options, by_wildcard_options,
                 by_edit_distance_options, by_terms_options, by_range_options>;

  using phrase_type = std::map<size_t, phrase_part>;

 public:
  using filter_type = by_phrase;

  // Insert phrase part into the phrase at a specified position.
  // Returns reference to the inserted phrase part.
  template<typename PhrasePart>
  PhrasePart& insert(size_t pos) {
    is_simple_term_only_ &= std::is_same_v<PhrasePart, by_term_options>;

    return std::get<PhrasePart>(phrase_[pos]);
  }

  // Insert phrase part into the phrase at a specified position
  // Returns reference to the inserted phrase part
  template<typename PhrasePart>
  PhrasePart& insert(PhrasePart&& t, size_t pos) {
    is_simple_term_only_ &= std::is_same_v<PhrasePart, by_term_options>;
    auto& part = (phrase_[pos] = std::forward<PhrasePart>(t));

    return std::get<std::decay_t<PhrasePart>>(part);
  }

  // Appends phrase part of type "PhrasePart" at a specified offset
  // "offs" from the end of the phrase.
  // Returns reference to the inserted phrase part.
  template<typename PhrasePart>
  PhrasePart& push_back(size_t offs = 0) {
    return insert(PhrasePart{}, next_pos() + offs);
  }

  // Appends phrase part of type "PhrasePart" at a specified offset
  // "offs" from the end of the phrase. Returns a reference to the
  // inserted phrase part
  template<typename PhrasePart>
  PhrasePart& push_back(PhrasePart&& t, size_t offs = 0) {
    return insert(std::forward<PhrasePart>(t), next_pos() + offs);
  }

  // Returns pointer to the phrase part of type "PhrasePart" located at a
  // specified position, nullptr if actual type mismatches a requested one
  template<typename PhrasePart>
  const PhrasePart* get(size_t pos) const noexcept {
    const auto it = phrase_.find(pos);

    if (it == phrase_.end()) {
      return nullptr;
    }

    return std::get_if<PhrasePart>(&it->second);
  }

  // Returns true is options are equal, false - otherwise
  bool operator==(const by_phrase_options& rhs) const noexcept {
    return phrase_ == rhs.phrase_;
  }

  // Clear phrase contents
  void clear() noexcept {
    phrase_.clear();
    is_simple_term_only_ = true;
  }

  // Returns true if phrase composed of simple terms only, false - otherwise
  bool simple() const noexcept { return is_simple_term_only_; }

  // Returns true if phrase is empty, false - otherwise
  bool empty() const noexcept { return phrase_.empty(); }

  // Returns size of the phrase
  size_t size() const noexcept { return phrase_.size(); }

  // Returns iterator referring to the first part of the phrase
  phrase_type::const_iterator begin() const noexcept { return phrase_.begin(); }

  // Returns iterator referring to past-the-end element of the phrase
  phrase_type::const_iterator end() const noexcept { return phrase_.end(); }

 private:
  size_t next_pos() const {
    return phrase_.empty() ? 0 : 1 + phrase_.rbegin()->first;
  }

  phrase_type phrase_;
  bool is_simple_term_only_{true};
};

class by_phrase : public FilterWithField<by_phrase_options> {
 public:
  static prepared::ptr Prepare(const PrepareContext& ctx,
                               std::string_view field,
                               const by_phrase_options& options);

  prepared::ptr prepare(const PrepareContext& ctx) const final {
    return Prepare(ctx.Boost(boost()), field(), options());
  }
};

}  // namespace irs

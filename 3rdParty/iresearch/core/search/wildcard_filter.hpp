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

#ifndef IRESEARCH_WILDCARD_FILTER_H
#define IRESEARCH_WILDCARD_FILTER_H

#include "search/filter.hpp"
#include "utils/string.hpp"

namespace iresearch {

class by_wildcard;
struct filter_visitor;

struct IRESEARCH_API by_wildcard_filter_options {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief search wildcard
  //////////////////////////////////////////////////////////////////////////////
  bstring term;

  bool operator==(const by_wildcard_filter_options& rhs) const noexcept {
    return term == rhs.term;
  }

  size_t hash() const noexcept {
    return std::hash<bstring>()(term);
  }
}; // by_wildcard_filter_options

////////////////////////////////////////////////////////////////////////////////
/// @struct by_prefix_options
/// @brief options for wildcard filter
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API by_wildcard_options : by_wildcard_filter_options {
  using filter_type = by_wildcard;
  using filter_options = by_wildcard_filter_options;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  size_t scored_terms_limit{1024};

  bool operator==(const by_wildcard_options& rhs) const noexcept {
    return filter_options::operator==(rhs) &&
      scored_terms_limit == rhs.scored_terms_limit;
  }

  size_t hash() const noexcept {
    return hash_combine(filter_options::hash(), scored_terms_limit);
  }
}; // by_wildcard_options

//////////////////////////////////////////////////////////////////////////////
/// @class by_wildcard
/// @brief user-side wildcard filter
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_wildcard final
    : public filter_base<by_wildcard_options> {
 public:
  DECLARE_FACTORY();

  static prepared::ptr prepare(
    const index_reader& index,
    const order::prepared& order,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& term,
    size_t scored_terms_limit);

  static field_visitor visitor(const bytes_ref& term);

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
      const index_reader& index,
      const order::prepared& order,
      boost_t boost,
      const attribute_provider* /*ctx*/) const override {
    return prepare(index, order, this->boost()*boost,
                   field(), options().term,
                   options().scored_terms_limit);
  }
}; // by_wildcard

}

namespace std {

template<>
struct hash<::iresearch::by_wildcard_options> {
  size_t operator()(const ::iresearch::by_wildcard_options& v) const noexcept {
    return v.hash();
  }
};

}

#endif // IRESEARCH_WILDCARD_FILTER_H

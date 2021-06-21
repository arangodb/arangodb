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

#ifndef IRESEARCH_PREFIX_FILTER_H
#define IRESEARCH_PREFIX_FILTER_H

#include "search/filter.hpp"
#include "utils/string.hpp"

namespace iresearch {

class by_prefix;
struct filter_visitor;

struct IRESEARCH_API by_prefix_filter_options {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief search prefix
  //////////////////////////////////////////////////////////////////////////////
  bstring term;

  bool operator==(const by_prefix_filter_options& rhs) const noexcept {
    return term == rhs.term;
  }

  size_t hash() const noexcept {
    return std::hash<bstring>()(term);
  }
}; // by_prefix_options

////////////////////////////////////////////////////////////////////////////////
/// @struct by_prefix_options
/// @brief options for prefix filter
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API by_prefix_options : by_prefix_filter_options {
  using filter_type = by_prefix;
  using filter_options = by_prefix_filter_options;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  size_t scored_terms_limit{1024};

  bool operator==(const by_prefix_options& rhs) const noexcept {
    return filter_options::operator==(rhs) &&
        scored_terms_limit == rhs.scored_terms_limit;
  }

  size_t hash() const noexcept {
    return hash_combine(filter_options::hash(), scored_terms_limit);
  }
}; // by_prefix_options

////////////////////////////////////////////////////////////////////////////////
/// @class by_prefix
/// @brief user-side prefix filter
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_prefix : public filter_base<by_prefix_options> {
 public:
  DECLARE_FACTORY();

  static prepared::ptr prepare(
    const index_reader& index,
    const order::prepared& ord,
    boost_t boost,
    const string_ref& field,
    const bytes_ref& prefix,
    size_t scored_terms_limit);

  static void visit(
    const sub_reader& segment,
    const term_reader& reader,
    const bytes_ref& prefix,
    filter_visitor& visitor);

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
      const index_reader& index,
      const order::prepared& ord,
      boost_t boost,
      const attribute_provider* /*ctx*/) const override {
    return prepare(index, ord, this->boost()*boost,
                   field(), options().term,
                   options().scored_terms_limit);
  }
}; // by_prefix

}

namespace std {

template<>
struct hash<::iresearch::by_prefix_options> {
  size_t operator()(const ::iresearch::by_prefix_options& v) const noexcept {
    return v.hash();
  }
};

}

#endif

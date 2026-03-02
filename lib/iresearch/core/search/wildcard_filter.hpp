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

#include "search/filter.hpp"
#include "utils/string.hpp"

namespace irs {

class by_wildcard;
struct filter_visitor;

struct by_wildcard_filter_options {
  bstring term;

  bool operator==(const by_wildcard_filter_options& rhs) const noexcept {
    return term == rhs.term;
  }
};

// Options for wildcard filter
struct by_wildcard_options : by_wildcard_filter_options {
  using filter_type = by_wildcard;
  using filter_options = by_wildcard_filter_options;

  // The maximum number of most frequent terms to consider for scoring
  size_t scored_terms_limit{1024};

  bool operator==(const by_wildcard_options& rhs) const noexcept {
    return filter_options::operator==(rhs) &&
           scored_terms_limit == rhs.scored_terms_limit;
  }
};

// User-side wildcard filter
class by_wildcard final : public FilterWithField<by_wildcard_options> {
 public:
  static prepared::ptr prepare(const PrepareContext& ctx,
                               std::string_view field, bytes_view term,
                               size_t scored_terms_limit);

  static field_visitor visitor(bytes_view term);

  prepared::ptr prepare(const PrepareContext& ctx) const final {
    return prepare(ctx.Boost(boost()), field(), options().term,
                   options().scored_terms_limit);
  }
};

}  // namespace irs

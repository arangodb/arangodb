////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "search/filter.hpp"
#include "utils/string.hpp"

namespace irs {

class by_ngram_similarity;

// Options for ngram similarity filter
struct by_ngram_similarity_options {
  using filter_type = by_ngram_similarity;

  std::vector<bstring> ngrams;
  float_t threshold{1.F};
#ifdef IRESEARCH_TEST
  bool allow_phrase{true};
#else
  static constexpr bool allow_phrase{true};
#endif

  bool operator==(const by_ngram_similarity_options& rhs) const noexcept {
    return ngrams == rhs.ngrams && threshold == rhs.threshold;
  }
};

class by_ngram_similarity
  : public FilterWithField<by_ngram_similarity_options> {
 public:
  static prepared::ptr Prepare(const PrepareContext& ctx,
                               std::string_view field_name,
                               const std::vector<irs::bstring>& ngrams,
                               float_t threshold, bool allow_phrase = true);

  prepared::ptr prepare(const PrepareContext& ctx) const final {
    return Prepare(ctx.Boost(boost()), field(), options().ngrams,
                   options().threshold, options().allow_phrase);
  }
};

}  // namespace irs

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

#include "ngram_similarity_filter.hpp"

#include "index/field_meta.hpp"
#include "index/index_reader.hpp"
#include "search/collectors.hpp"
#include "search/ngram_similarity_query.hpp"
#include "search/phrase_filter.hpp"
#include "search/terms_filter.hpp"
#include "shared.hpp"

namespace irs {

filter::prepared::ptr by_ngram_similarity::Prepare(
  const PrepareContext& ctx, std::string_view field_name,
  const std::vector<irs::bstring>& ngrams, float_t threshold,
  bool allow_phrase) {
  if (ngrams.empty() || field_name.empty()) {
    // empty field or terms or invalid threshold
    return prepared::empty();
  }
  const auto terms_count = ngrams.size();

  threshold = std::clamp(threshold, 0.f, 1.f);
  const auto min_match_count =
    std::clamp(static_cast<size_t>(
                 std::ceil(static_cast<float_t>(terms_count) * threshold)),
               size_t{1}, terms_count);
  if (ctx.scorers.empty() && 1 == min_match_count) {
    irs::by_terms_options options;
    for (const auto& term : ngrams) {
      options.terms.emplace(term, irs::kNoBoost);
    }
    return by_terms::Prepare(ctx, field_name, options);
  }

  if (allow_phrase && min_match_count == terms_count) {
    irs::by_phrase_options options;
    for (size_t i = 0; const auto& ngram : ngrams) {
      options.insert(by_term_options{ngram}, i++);
    }
    return by_phrase::Prepare(ctx, field_name, options);
  }

  NGramStates query_states{ctx.memory, ctx.index.size()};

  // per segment terms states
  ManagedVector<seek_cookie::ptr> term_states{{ctx.memory}};
  term_states.reserve(terms_count);

  // prepare ngrams stats
  field_collectors field_stats{ctx.scorers};
  term_collectors term_stats{ctx.scorers, terms_count};

  for (const auto& segment : ctx.index) {
    // get term dictionary for field
    const term_reader* field = segment.field(field_name);

    if (!field) {
      continue;
    }

    // check required features
    if (NGramSimilarityQuery::kRequiredFeatures !=
        (field->meta().index_features &
         NGramSimilarityQuery::kRequiredFeatures)) {
      continue;
    }

    // collect field statistics once per segment
    field_stats.collect(segment, *field);
    size_t term_idx = 0;
    size_t count_terms = 0;
    auto term = field->iterator(SeekMode::NORMAL);
    for (const auto& ngram : ngrams) {
      auto& state = term_states.emplace_back();
      if (term->seek(ngram)) {
        // read term attributes
        term->read();
        // collect statistics
        term_stats.collect(segment, *field, term_idx, *term);
        state = term->cookie();
        ++count_terms;
      }

      ++term_idx;
    }

    if (count_terms < min_match_count) {
      // we have not found enough terms
      term_states.clear();
      continue;
    }

    auto& state = query_states.insert(segment);
    state.terms = std::move(term_states);
    state.field = field;

    term_states.reserve(terms_count);
  }

  if (query_states.empty()) {
    return prepared::empty();
  }

  bstring stats(ctx.scorers.stats_size(), 0);
  auto* stats_buf = stats.data();

  for (size_t term_idx = 0; term_idx < terms_count; ++term_idx) {
    term_stats.finish(stats_buf, term_idx, field_stats, ctx.index);
  }

  return memory::make_tracked<NGramSimilarityQuery>(
    ctx.memory, min_match_count, std::move(query_states), std::move(stats),
    ctx.boost);
}

}  // namespace irs

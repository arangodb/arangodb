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

#include "search/scorers.hpp"

namespace irs {

// BM25 similarity
// bm25(doc, term) = idf(term) * ((k + 1) * tf(doc, term)) / (k * (1 - b + b *
// |doc|/avgDL) + tf(doc, term))
//
// Inverted document frequency
// idf(term) = log(1 + (#documents with this field - #documents with this term +
// 0.5)/(#documents with this term + 0.5))
//
// Term frequency
//   Norm2: tf(doc, term) = frequency(doc, term);
//   Norm:  tf(doc, term) = sqrt(frequency(doc, term));
//
// Document length
//   Norm2: |doc| # of terms in a field within a document
//   Norm:  |doc| = 1 / sqrt(# of terms in a field within a document)
//
// Average document length
// avgDL = sum(field_term_count) / (#documents with this field)

struct BM25Stats {
  // precomputed idf value
  float_t idf;
  // precomputed k*(1-b)
  float_t norm_const;
  // precomputed k*b/avgD
  float_t norm_length;
  // precomputed 1/(k*(1-b+b*|doc|/avgDL)) for |doc| E [0..255]
  float_t norm_cache[256];
};

class BM25 final : public irs::ScorerBase<BM25, BM25Stats> {
 public:
  static constexpr std::string_view type_name() noexcept { return "bm25"; }

  static constexpr float_t K() noexcept { return 1.2f; }

  static constexpr float_t B() noexcept { return 0.75f; }

  static constexpr bool BOOST_AS_SCORE() noexcept { return false; }

  static void init();  // for trigering registration in a static build

  BM25(float_t k = K(), float_t b = B(),
       bool boost_as_score = BOOST_AS_SCORE()) noexcept
    : k_{k}, b_{b}, boost_as_score_{boost_as_score} {}

  void collect(byte_type* stats_buf, const irs::FieldCollector* field,
               const irs::TermCollector* term) const final;

  IndexFeatures index_features() const noexcept final {
    return IndexFeatures::FREQ;
  }

  void get_features(feature_set_t& features) const final;

  FieldCollector::ptr prepare_field_collector() const final;

  ScoreFunction prepare_scorer(const ColumnProvider& segment,
                               const feature_map_t& features,
                               const byte_type* query_stats,
                               const attribute_provider& doc_attrs,
                               score_t boost) const final;

  WandWriter::ptr prepare_wand_writer(size_t max_levels) const final;

  WandSource::ptr prepare_wand_source() const final;

  TermCollector::ptr prepare_term_collector() const final;

  WandType wand_type() const noexcept final;

  bool equals(const Scorer& other) const noexcept final;

  float_t k() const noexcept { return k_; }

  float_t b() const noexcept { return b_; }

  bool use_boost_as_score() const noexcept { return boost_as_score_; }

  bool IsBM15() const noexcept { return b_ == 0.f; }

  bool IsBM11() const noexcept { return b_ == 1.f; }

  bool IsBM1() const noexcept { return k_ == 0.f; }

  bool NeedsNorm() const noexcept { return !IsBM1() && !IsBM15(); }

 private:
  float_t k_;
  float_t b_;
  bool boost_as_score_;
};

}  // namespace irs

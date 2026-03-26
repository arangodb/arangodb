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

struct TFIDFStats {
  float_t value;
};

class TFIDF final : public irs::ScorerBase<TFIDF, TFIDFStats> {
 public:
  static constexpr std::string_view type_name() noexcept { return "tfidf"; }

  static constexpr bool WITH_NORMS() noexcept { return false; }

  static constexpr bool BOOST_AS_SCORE() noexcept { return false; }

  static void init();

  explicit TFIDF(bool normalize = WITH_NORMS(),
                 bool boost_as_score = BOOST_AS_SCORE()) noexcept
    : normalize_{normalize}, boost_as_score_{boost_as_score} {}

  void collect(byte_type* stats_buf, const irs::FieldCollector* field,
               const irs::TermCollector* term) const final;

  IndexFeatures index_features() const noexcept final {
    return IndexFeatures::FREQ;
  }

  void get_features(feature_set_t& features) const final;

  FieldCollector::ptr prepare_field_collector() const final;

  ScoreFunction prepare_scorer(const ColumnProvider& segment,
                               const feature_map_t& features,
                               const byte_type* stats_buf,
                               const attribute_provider& doc_attrs,
                               score_t boost) const final;

  TermCollector::ptr prepare_term_collector() const final;

  WandWriter::ptr prepare_wand_writer(size_t max_levels) const final;

  WandSource::ptr prepare_wand_source() const final;

  WandType wand_type() const noexcept final;

  bool equals(const Scorer& other) const noexcept final;

  bool normalize() const noexcept { return normalize_; }

  bool use_boost_as_score() const noexcept { return boost_as_score_; }

 private:
  bool normalize_;
  bool boost_as_score_;
};

}  // namespace irs

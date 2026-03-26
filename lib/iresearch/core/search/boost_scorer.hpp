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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "scorers.hpp"

namespace irs {

struct BoostScore final : ScorerBase<BoostScore, void> {
  static constexpr std::string_view type_name() noexcept {
    return "boostscore";
  }

  static void init();

  ScoreFunction prepare_scorer(const ColumnProvider& /*segment*/,
                               const feature_map_t& /*features*/,
                               const byte_type* /*stats*/,
                               const attribute_provider& attrs,
                               score_t boost) const final;

  IndexFeatures index_features() const noexcept final {
    return IndexFeatures::NONE;
  }
};

}  // namespace irs

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

#include "boost_scorer.hpp"

namespace irs {
namespace {

Scorer::ptr make_json(std::string_view /*args*/) {
  return std::make_unique<BoostScore>();
}

struct volatile_boost_score_ctx final : score_ctx {
  volatile_boost_score_ctx(const filter_boost* volatile_boost,
                           score_t boost) noexcept
    : boost{boost}, volatile_boost{volatile_boost} {
    IRS_ASSERT(volatile_boost);
  }

  score_t boost;
  const filter_boost* volatile_boost;
};
}  // namespace

ScoreFunction BoostScore::prepare_scorer(const ColumnProvider& /*segment*/,
                                         const feature_map_t& /*features*/,
                                         const byte_type* /*stats*/,
                                         const attribute_provider& attrs,
                                         score_t boost) const {
  const auto* volatile_boost = irs::get<irs::filter_boost>(attrs);

  if (volatile_boost == nullptr) {
    return ScoreFunction::Constant(boost);
  }

  return ScoreFunction::Make<volatile_boost_score_ctx>(
    [](score_ctx* ctx, score_t* res) noexcept {
      auto& state = *static_cast<volatile_boost_score_ctx*>(ctx);
      *res = state.volatile_boost->value * state.boost;
    },
    ScoreFunction::DefaultMin, volatile_boost, boost);
}

void BoostScore::init() { REGISTER_SCORER_JSON(BoostScore, make_json); }

}  // namespace irs

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#include "scorer.hpp"

#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "shared.hpp"

namespace irs {

uint8_t Scorer::compatible(WandType index, WandType query) noexcept {
  auto bin_case = [](WandType index, WandType query) noexcept -> uint8_t {
    return (static_cast<uint8_t>(index) * 8) + static_cast<uint8_t>(query);
  };
  switch (bin_case(index, query)) {
    // no needed wand data
    case bin_case(WandType::kNone, WandType::kNone):
    case bin_case(WandType::kNone, WandType::kDivNorm):
    case bin_case(WandType::kNone, WandType::kMaxFreq):
    case bin_case(WandType::kNone, WandType::kMinNorm):
    case bin_case(WandType::kDivNorm, WandType::kNone):
    case bin_case(WandType::kMaxFreq, WandType::kNone):
    case bin_case(WandType::kMinNorm, WandType::kNone):
      IRS_ASSERT(false);
      [[fallthrough]];
    // DivNorm very precise and is not compatible with other types
    case bin_case(WandType::kDivNorm, WandType::kMaxFreq):
    case bin_case(WandType::kDivNorm, WandType::kMinNorm):
      return 0;
    // MaxFreq suitable for any other type
    case bin_case(WandType::kMaxFreq, WandType::kDivNorm):
    case bin_case(WandType::kMaxFreq, WandType::kMinNorm):
    // MinNorm suitable for any score
    case bin_case(WandType::kMinNorm, WandType::kMaxFreq):
      return 1;
    case bin_case(WandType::kMinNorm, WandType::kDivNorm):
      return 2;
    case bin_case(WandType::kDivNorm, WandType::kDivNorm):
    case bin_case(WandType::kMaxFreq, WandType::kMaxFreq):
    case bin_case(WandType::kMinNorm, WandType::kMinNorm):
      return std::numeric_limits<uint8_t>::max();
  }
  return 0;
}

size_t Scorers::PushBack(const Scorer& scorer) {
  const auto [bucket_stats_size, bucket_stats_align] = scorer.stats_size();
  IRS_ASSERT(bucket_stats_align <= alignof(std::max_align_t));
  // math::is_power2(0) returns true
  IRS_ASSERT(math::is_power2(bucket_stats_align));

  stats_size_ = memory::align_up(stats_size_, bucket_stats_align);
  features_ |= scorer.index_features();
  buckets_.emplace_back(scorer, stats_size_);
  stats_size_ += memory::align_up(bucket_stats_size, bucket_stats_align);

  return bucket_stats_align;
}

REGISTER_ATTRIBUTE(filter_boost);

const Scorers Scorers::kUnordered;

}  // namespace irs

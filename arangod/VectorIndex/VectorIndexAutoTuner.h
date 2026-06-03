////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "Basics/ResultT.h"
#include "Inspection/Status.h"

namespace faiss {
class IndexIVF;
}

namespace arangodb::vector {

inline constexpr std::int64_t kDefaultAutoTuneR{10};
inline constexpr double kDefaultAutoTuneTargetRecall{0.9};
inline constexpr double kAutoTuneRecallEpsilon{5e-4};
inline constexpr std::size_t kAutoTuneSampleCap{1024};

struct AutotuneParams {
  std::int64_t R{kDefaultAutoTuneR};
  double targetRecall{kDefaultAutoTuneTargetRecall};
  std::size_t sampleSize{kAutoTuneSampleCap};

  template<class Inspector>
  friend inline auto inspect(Inspector& f, AutotuneParams& x) {
    return f.object(x).fields(
        f.field("topK", x.R)
            .fallback(kDefaultAutoTuneR)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"'topK' must be a positive integer"};
              }
              return inspection::Status::Success{};
            }),
        f.field("targetRecall", x.targetRecall)
            .fallback(kDefaultAutoTuneTargetRecall)
            .invariant([](auto value) -> inspection::Status {
              if (value <= 0.0 || value > 1.0) {
                return {"'targetRecall' must be a number in (0, 1]"};
              }
              return inspection::Status::Success{};
            }),
        f.field("sampleSize", x.sampleSize)
            .fallback(kAutoTuneSampleCap)
            .invariant([](auto value) -> inspection::Status {
              if (value < 1) {
                return {"'sampleSize' must be a positive integer"};
              }
              return inspection::Status::Success{};
            }));
  }
};

// Sweep nprobe over powers of two via FAISS's ParameterSpace::explore, pick
// the smallest value whose recall@R meets `targetRecall`, or fall back to
// the highest-recall trial.
// The results of autoTuneNPRove are valied for a given R (topK during search)
// and the target recall
ResultT<std::int64_t> autoTuneNProbe(
    faiss::IndexIVF& index, std::span<float const> querySet,
    void* invertedListContext = nullptr, std::int64_t R = kDefaultAutoTuneR,
    double targetRecall = kDefaultAutoTuneTargetRecall);

}  // namespace arangodb::vector

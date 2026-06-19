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
#include "VectorIndex/VectorIndexDefinition.h"

namespace faiss {
class IndexIVF;
}

namespace arangodb::vector {

inline constexpr std::int64_t kDefaultAutoTuneR{10};
inline constexpr double kDefaultAutoTuneMinRecall{0.9};
inline constexpr double kAutoTuneRecallEpsilon{5e-4};
inline constexpr std::size_t kAutoTuneSampleCap{1024};

// Fixed planning parameters for the Wilson sample-size derivation. The
// confidence level is expressed directly as its two-sided normal quantile z*
// (0.90 -> 1.645, 0.95 -> 1.960, 0.99 -> 2.576) to avoid an inverse-normal CDF.
inline constexpr double kAutoTuneConfidenceZ{2.5758293035489004};  // 99%
inline constexpr double kAutoTuneRecallTolerance{0.03};

// Smallest sample size n whose Wilson score interval half-width at proportion
// `p` equals `m`, for the two-sided normal quantile `z`. Wilson stays
// boundary-aware near the high-recall end where the textbook Wald rule
// collapses (Brown, Cai & DasGupta 2001,
// https://doi.org/10.1214/ss/1009213286). Requires p in (0, 1), m > 0, z > 0.
std::size_t wilsonSampleSize(double p, double z, double m);

struct AutotuneParams {
  // Result set size the table is optimized for (the query-time LIMIT).
  std::int64_t R{kDefaultAutoTuneR};
  // Lowest recall the table must stay valid down to. Drives the sample size
  // (via Wilson) and is recorded on the table as the validity floor;
  // targetRecall is chosen per query, not here.
  double minRecall{kDefaultAutoTuneMinRecall};

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
        f.field("minRecall", x.minRecall)
            .fallback(kDefaultAutoTuneMinRecall)
            .invariant([](auto value) -> inspection::Status {
              if (value <= 0.0 || value > 1.0) {
                return {"'minRecall' must be a number in (0, 1]"};
              }
              return inspection::Status::Success{};
            }));
  }
};

// Sweep the index's search parameters via FAISS's ParameterSpace::explore and
// return the full operating-point table: every Pareto-optimal configuration
// paired with the recall it achieves
ResultT<OperatingPointTable> autoTuneTable(
    faiss::IndexIVF& index, std::span<float const> querySet,
    void* invertedListContext = nullptr, std::int64_t R = kDefaultAutoTuneR,
    double minRecall = kDefaultAutoTuneMinRecall);

}  // namespace arangodb::vector

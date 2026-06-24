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
#include <memory>
#include <span>

#include "Basics/ResultT.h"
#include "Inspection/Status.h"
#include "VectorIndex/VectorIndexDefinition.h"

namespace faiss {
class IndexIVF;
struct SearchParametersIVF;
}  // namespace faiss

namespace arangodb {
struct ResourceMonitor;
}

namespace arangodb::vector {

inline constexpr std::int64_t kDefaultAutoTuneR{10};
inline constexpr double kAutoTuneRecallEpsilon{5e-4};

// confidence as its two-sided normal quantile z* (0.99 -> 2.576), avoiding an
// inverse-normal CDF.
inline constexpr double kAutoTuneConfidenceZ{2.5758293035489004};  // 99%
inline constexpr double kAutoTuneRecallTolerance{0.01};

// Smallest sample size n whose Wilson score interval half-width at proportion
// `p` equals `m`, for the two-sided normal quantile `z` (Brown, Cai & DasGupta
// 2001, https://doi.org/10.1214/ss/1009213286). Requires p in (0,1), m,z > 0.
std::size_t wilsonSampleSize(double p, double z, double m);

struct AutotuneParams {
  std::int64_t R{kDefaultAutoTuneR};  // topK the table is optimized for
  // Recall floor: drives the sample size and is the table's validity floor.
  // Required from the caller; there is no server-side default.
  double minRecall{};

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
            .invariant([](auto value) -> inspection::Status {
              if (value <= 0.0 || value > 1.0) {
                return {"'minRecall' must be a number in (0, 1]"};
              }
              return inspection::Status::Success{};
            }));
  }
};

// Sweep the index's search parameters and return the full Pareto front as an
// operating-point table for `R` (topK), tuned down to `minRecall`. The
// ground-truth scratch buffers are charged to `resourceMonitor`.
ResultT<OperatingPointTable> autoTuneTable(faiss::IndexIVF& index,
                                           std::span<float const> querySet,
                                           ResourceMonitor& resourceMonitor,
                                           void* invertedListContext,
                                           std::int64_t R, double minRecall);

// Cheapest operating point reaching `targetRecall` for `topK`. Fails if no
// table was tuned for `topK` or `targetRecall` exceeds the table's range.
ResultT<OperatingPoint> selectOperatingPoint(
    std::vector<OperatingPointTable> const& tables, std::int64_t topK,
    double targetRecall);

// Build per-call SearchParameters from a stored FAISS combination key
// (e.g. "nprobe=8,ht=20").
ResultT<std::unique_ptr<faiss::SearchParametersIVF>>
makeSearchParametersFromKey(faiss::IndexIVF const& index,
                            std::string const& key);

}  // namespace arangodb::vector

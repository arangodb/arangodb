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

#include <cstdint>
#include <span>

#include "Basics/ResultT.h"

namespace faiss {
class IndexIVF;
}

namespace arangodb::vector {

inline constexpr std::int64_t kdefaultAutoTuneR{10};
inline constexpr double kdefaultAutoTuneTargetRecall{0.9};
inline constexpr double kAutoTuneRecallEpsilon{5e-4};

// Sweep nprobe over powers of two via FAISS's ParameterSpace::explore, pick
// the smallest value whose recall@R meets `targetRecall`, or fall back to
// the highest-recall trial. `invertedListContext` is forwarded to every
// search call (incl. per-trial searches inside explore) via
// SearchParametersIVF::inverted_list_context.
ResultT<std::int64_t> autoTuneNProbe(
    faiss::IndexIVF& index, std::span<float const> querySet,
    void* invertedListContext = nullptr, std::int64_t R = kdefaultAutoTuneR,
    double targetRecall = kdefaultAutoTuneTargetRecall);

}  // namespace arangodb::vector

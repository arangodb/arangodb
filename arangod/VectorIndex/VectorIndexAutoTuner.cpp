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

#include "VectorIndex/VectorIndexAutoTuner.h"

#include <chrono>
#include <cstddef>
#include <format>
#include <string>
#include <vector>

#include "Basics/ScopeGuard.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <faiss/AutoTune.h>
#include <faiss/IndexIVF.h>
#include <faiss/impl/FaissException.h>

namespace arangodb::vector {

namespace {

struct Trial {
  std::int64_t nprobe;
  double recall;
  double seconds;
};

// Mirrors faiss/AutoTune.cpp:358-367: powers of two strictly less than
// nlist, capped at 13 entries (so max 4096).
std::vector<std::int64_t> nProbeCandidates(std::size_t nlist) {
  std::vector<std::int64_t> out;
  for (int i = 0; i < 13; ++i) {
    auto const v = std::int64_t{1} << i;
    if (static_cast<std::size_t>(v) >= nlist) {
      break;
    }
    out.push_back(v);
  }
  return out;
}

std::string formatTrials(std::vector<Trial> const& trials,
                         Trial const* chosen) {
  std::string out;
  for (auto const& t : trials) {
    if (!out.empty()) {
      out += "; ";
    }
    out += std::format("nprobe={} perf={:.3f} t={:.3f}s{}", t.nprobe, t.recall,
                       t.seconds,
                       (chosen != nullptr && &t == chosen) ? " <-chosen" : "");
  }
  return out;
}

}  // namespace

ResultT<std::int64_t> autoTuneNProbe(faiss::IndexIVF& index,
                                     std::span<float const> querySet,
                                     void* invertedListContext, std::int64_t R,
                                     double targetRecall) {
  TRI_ASSERT(R >= 1);
  TRI_ASSERT(targetRecall > 0.0 && targetRecall <= 1.0);
  TRI_ASSERT(index.d > 0);

  auto const d = static_cast<std::size_t>(index.d);
  TRI_ASSERT(querySet.size() > 0 && querySet.size() % d == 0)
      << "autotune query set size is not a positive multiple of the index "
         "dimension";
  auto const numberOfQueries = static_cast<faiss::idx_t>(querySet.size() / d);

  LOG_TOPIC("e16af", INFO, Logger::ENGINES)
      << "Autotune starting: numberOfQueries=" << numberOfQueries
      << " nlist=" << index.nlist << " R=" << R
      << " targetRecall=" << targetRecall << ".";
  auto const startTime = std::chrono::steady_clock::now();
  auto elapsedSecs = [&] {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                         startTime)
        .count();
  };

  Result outcome;
  auto failLog = ScopeGuard([&]() noexcept {
    LOG_TOPIC("e16ae", WARN, Logger::ENGINES)
        << "Autotune failed after " << elapsedSecs()
        << "s: " << outcome.errorMessage();
  });

  // Ground truth: exhaustive IVF (nprobe = nlist).
  // TODO(jbajic) there might be a bettter way to find ground truth
  std::vector<faiss::idx_t> gtI(static_cast<std::size_t>(numberOfQueries) * R);
  std::vector<float> gtD(static_cast<std::size_t>(numberOfQueries) * R);
  faiss::SearchParametersIVF params;
  params.inverted_list_context = invertedListContext;
  params.nprobe = index.nlist;
  auto const gtStart = std::chrono::steady_clock::now();
  try {
    index.search(numberOfQueries, querySet.data(), R, gtD.data(), gtI.data(),
                 &params);
  } catch (faiss::FaissException const& e) {
    outcome = Result{
        TRI_ERROR_INTERNAL,
        std::format("autotune ground-truth search failed: {}", e.what())};
    return outcome;
  }
  LOG_TOPIC("e16b1", INFO, Logger::ENGINES)
      << "Autotune ground truth (nprobe=" << index.nlist << ") done in "
      << std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                       gtStart)
             .count()
      << "s.";

  faiss::IntersectionCriterion crit(numberOfQueries, R);
  crit.set_groundtruth(static_cast<int>(R), gtD.data(), gtI.data());

  auto const candidates = nProbeCandidates(index.nlist);
  if (candidates.empty()) {
    outcome = Result{TRI_ERROR_INTERNAL,
                     "autotune produced no candidates (nlist too small)"};
    return outcome;
  }

  std::vector<Trial> trials;
  trials.reserve(candidates.size());
  std::vector<faiss::idx_t> I(static_cast<std::size_t>(numberOfQueries) * R);
  std::vector<float> D(static_cast<std::size_t>(numberOfQueries) * R);
  for (auto const nprobe : candidates) {
    params.nprobe = nprobe;
    auto const trialStart = std::chrono::steady_clock::now();
    try {
      index.search(numberOfQueries, querySet.data(), R, D.data(), I.data(),
                   &params);
    } catch (faiss::FaissException const& e) {
      outcome = Result{
          TRI_ERROR_INTERNAL,
          std::format("autotune trial nprobe={} failed: {}", nprobe, e.what())};
      return outcome;
    }
    auto const trialSecs = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - trialStart)
                               .count();
    auto const recall = crit.evaluate(D.data(), I.data());
    trials.push_back(Trial{nprobe, recall, trialSecs});
    LOG_TOPIC("e16b2", INFO, Logger::ENGINES)
        << "Autotune trial " << trials.size() << "/" << candidates.size()
        << ": nprobe=" << nprobe << " perf=" << recall << " t=" << trialSecs
        << "s.";
  }

  Trial const* chosen = nullptr;
  Trial const* fallback = nullptr;
  for (auto const& t : trials) {
    if (fallback == nullptr || t.recall > fallback->recall) {
      fallback = &t;
    }
    if (chosen == nullptr && t.recall >= targetRecall) {
      chosen = &t;
    }
  }
  if (chosen == nullptr) {
    chosen = fallback;
  }
  TRI_ASSERT(chosen != nullptr);

  failLog.cancel();
  index.nprobe = chosen->nprobe;
  LOG_TOPIC("e16ad", INFO, Logger::ENGINES)
      << "Autotune chose nprobe=" << chosen->nprobe << " (target recall@" << R
      << "≥" << targetRecall << ", took " << elapsedSecs()
      << "s). Operating points: " << formatTrials(trials, chosen);
  return chosen->nprobe;
}

}  // namespace arangodb::vector

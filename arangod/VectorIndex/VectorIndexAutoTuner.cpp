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
#include <exception>
#include <format>
#include <string>
#include <unordered_set>
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

// FAISS formats single-parameter keys as "nprobe=32" (see
// ParameterSpace::combination_name in faiss/AutoTune.cpp).
ResultT<std::int64_t> parseNProbeKey(std::string const& key) {
  auto const eq = key.find('=');
  if (eq == std::string::npos) {
    return Result{TRI_ERROR_INTERNAL,
                  std::format("unexpected autotune key '{}'", key)};
  }
  try {
    return std::stoll(key.substr(eq + 1));
  } catch (std::exception const&) {
    return Result{TRI_ERROR_INTERNAL,
                  std::format("could not parse autotune key '{}'", key)};
  }
}

// TODO(jbajic) there might be a better way to find ground truth.
// FAISS's search API requires a distances buffer, but the criteria we use
// (IntersectionCriterion, OneRecallAtRCriterion) never read it — so we
// scratch it locally and return only the IDs.
ResultT<std::vector<faiss::idx_t>> computeGroundTruth(
    faiss::IndexIVF& index, std::span<float const> querySet,
    faiss::idx_t numberOfQueries, std::int64_t R, void* invertedListContext) {
  auto const total = static_cast<std::size_t>(numberOfQueries) * R;
  std::vector<faiss::idx_t> ids(total);
  std::vector<float> distancesScratch(total);
  faiss::SearchParametersIVF params;
  params.inverted_list_context = invertedListContext;
  params.nprobe = index.nlist;
  auto const start = std::chrono::steady_clock::now();
  try {
    index.search(numberOfQueries, querySet.data(), R, distancesScratch.data(),
                 ids.data(), &params);
  } catch (faiss::FaissException const& e) {
    return Result{
        TRI_ERROR_INTERNAL,
        std::format("autotune ground-truth search failed: {}", e.what())};
  }
  LOG_TOPIC("e16b1", INFO, Logger::ENGINES)
      << "Autotune ground truth (nprobe=" << index.nlist << ") done in "
      << std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
             .count()
      << "s.";
  return ids;
}

// Set up a FAISS ParameterSpace for an nprobe sweep on `index` (powers of
// two up to nlist) and run a full sweep against `crit`. Returns the
// operating points or a `Result` error if explore throws or yields no
// Pareto points to choose from.
ResultT<faiss::OperatingPoints> exploreParameterSpace(
    faiss::IndexIVF& index, std::span<float const> querySet,
    faiss::idx_t numberOfQueries, void* invertedListContext,
    faiss::IntersectionCriterion const& crit) {
  faiss::SearchParametersIVF trialParams;
  trialParams.inverted_list_context = invertedListContext;
  faiss::ParameterSpace ps;
#if ARANGODB_ENABLE_MAINTAINER_MODE
  ps.verbose = true;
#else
  ps.verbose = false;
#endif
  ps.initialize(&index);
  ps.n_experiments = 0;  // do full trials TODO(jbajic): maybe add a cap and/or
                         // randomize if we have too many?
  ps.batchsize = static_cast<std::size_t>(numberOfQueries);

  faiss::OperatingPoints ops;
  try {
    ps.explore(&index, static_cast<std::size_t>(numberOfQueries),
               querySet.data(), crit, &ops, &trialParams);
  } catch (faiss::FaissException const& e) {
    return Result{TRI_ERROR_INTERNAL,
                  std::format("autotune explore failed: {}", e.what())};
  }

  if (ops.optimal_pts.empty()) {
    return Result{TRI_ERROR_INTERNAL, "autotune produced no operating points"};
  }

  return ops;
}

// Returns the smallest-perf Pareto point whose recall meets `targetRecall`
// (within kAutoTuneRecallEpsilon), or the highest-perf Pareto point if
// none qualify. Empty-keyed entries (FAISS's default dummy at perf=0) are
// skipped. Returns nullptr only if `ops` has no non-empty Pareto points.
faiss::OperatingPoint const* pickParetoOperatingPoint(
    faiss::OperatingPoints const& ops, double targetRecall) {
  faiss::OperatingPoint const* chosen = nullptr;
  faiss::OperatingPoint const* fallback = nullptr;
  for (auto const& op : ops.optimal_pts) {
    if (op.key.empty()) {
      continue;
    }
    if (fallback == nullptr || op.perf > fallback->perf) {
      fallback = &op;
    }
    if (chosen == nullptr && op.perf >= targetRecall - kAutoTuneRecallEpsilon) {
      chosen = &op;
    }
  }
  return chosen != nullptr ? chosen : fallback;
}

// Single-line summary of every trial in `ops.all_pts`. Pareto-optimal points
// get `*`, the chosen one gets ` <-chosen`. LOG_TOPIC escapes newlines so we
// join with `; `.
std::string formatOperatingPoints(faiss::OperatingPoints const& ops,
                                  faiss::OperatingPoint const* chosen) {
  std::unordered_set<std::int64_t> paretoCnos;
  paretoCnos.reserve(ops.optimal_pts.size());
  for (auto const& op : ops.optimal_pts) {
    paretoCnos.insert(op.cno);
  }
  std::string out;
  for (auto const& op : ops.all_pts) {
    if (op.key.empty()) {
      continue;
    }
    if (!out.empty()) {
      out += "; ";
    }
    bool const isPareto = paretoCnos.contains(op.cno);
    bool const isChosen = (chosen != nullptr && op.cno == chosen->cno);
    out += std::format("{} perf={:.3f} t={:.3f}s{}{}", op.key, op.perf, op.t,
                       isPareto ? " *" : "", isChosen ? " <-chosen" : "");
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

  auto gt = computeGroundTruth(index, querySet, numberOfQueries, R,
                               invertedListContext);
  if (gt.fail()) {
    outcome = std::move(gt).result();
    return outcome;
  }

  faiss::IntersectionCriterion crit(numberOfQueries, R);
  crit.set_groundtruth(static_cast<int>(R), nullptr, gt.get().data());

  auto opsRes = exploreParameterSpace(index, querySet, numberOfQueries,
                                      invertedListContext, crit);
  if (opsRes.fail()) {
    outcome = std::move(opsRes).result();
    return outcome;
  }
  auto const& ops = opsRes.get();

  auto const* chosen = pickParetoOperatingPoint(ops, targetRecall);
  if (chosen == nullptr) {
    outcome = Result{TRI_ERROR_INTERNAL,
                     "autotune found no non-empty operating points"};
    return outcome;
  }

  auto chosenNProbe = parseNProbeKey(chosen->key);
  if (chosenNProbe.fail()) {
    outcome = std::move(chosenNProbe).result();
    return outcome;
  }

  failLog.cancel();
  index.nprobe = chosenNProbe.get();
  LOG_TOPIC("e16ad", INFO, Logger::ENGINES)
      << "Autotune chose nprobe=" << chosenNProbe.get() << " (target recall@"
      << R << "≥" << targetRecall << ", took " << elapsedSecs()
      << "s). Operating points: " << formatOperatingPoints(ops, chosen);
  return chosenNProbe.get();
}

}  // namespace arangodb::vector

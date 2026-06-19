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

#include "VectorIndex/AutoTuner.h"

#include <chrono>
#include <cmath>
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

// Use nLists as nProbe to get ground truth. Distance is unused
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

std::size_t wilsonSampleSize(double p, double z, double m) {
  TRI_ASSERT(p > 0.0 && p < 1.0);
  TRI_ASSERT(z > 0.0);
  TRI_ASSERT(m > 0.0);

  // Setting the Wilson half-width equal to m and solving for n yields a
  // quadratic A n^2 + B n + C = 0 (see proposal). C < 0 here (m^2 < 1/4), so
  // the discriminant is positive and the larger root is the n we want.
  double const z2 = z * z;
  double const z4 = z2 * z2;
  double const m2 = m * m;
  double const a = m2;
  double const b = z2 * (2.0 * m2 - p * (1.0 - p));
  double const c = z4 * (m2 - 0.25);
  double const n = (-b + std::sqrt(b * b - 4.0 * a * c)) / (2.0 * a);
  return static_cast<std::size_t>(std::ceil(n));
}

ResultT<OperatingPointTable> autoTuneTable(faiss::IndexIVF& index,
                                           std::span<float const> querySet,
                                           void* invertedListContext,
                                           std::int64_t R, double minRecall) {
  TRI_ASSERT(R >= 1);
  TRI_ASSERT(minRecall > 0.0 && minRecall <= 1.0);
  TRI_ASSERT(index.d > 0);

  auto const d = static_cast<std::size_t>(index.d);
  TRI_ASSERT(querySet.size() > 0 && querySet.size() % d == 0)
      << "autotune query set size is not a positive multiple of the index "
         "dimension";
  auto const numberOfQueries = static_cast<faiss::idx_t>(querySet.size() / d);

  LOG_TOPIC("e16af", INFO, Logger::ENGINES)
      << "Autotune starting: numberOfQueries=" << numberOfQueries
      << " nlist=" << index.nlist << " R=" << R << " minRecall=" << minRecall
      << ".";
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

  // Keep the whole Pareto front: every recall level paired with its fastest
  // configuration. FAISS orders optimal_pts by ascending recall (== cost);
  // skip the empty-keyed default dummy at perf=0.
  OperatingPointTable table;
  table.topK = R;
  table.minRecall = minRecall;
  table.points.reserve(ops.optimal_pts.size());
  for (auto const& op : ops.optimal_pts) {
    if (op.key.empty()) {
      continue;
    }
    table.points.push_back(OperatingPoint{op.perf, op.key, op.t});
  }

  if (table.points.empty()) {
    outcome = Result{TRI_ERROR_INTERNAL,
                     "autotune found no non-empty operating points"};
    return outcome;
  }

  failLog.cancel();
  double const bestRecall = table.points.back().recall;
  if (bestRecall < minRecall - kAutoTuneRecallEpsilon) {
    LOG_TOPIC("e16ac", WARN, Logger::ENGINES)
        << "Autotune could not reach minRecall=" << minRecall
        << " for topK=" << R << "; best attainable recall=" << bestRecall
        << ". Returning best-attainable table.";
  }
  LOG_TOPIC("e16ad", INFO, Logger::ENGINES)
      << "Autotune produced " << table.points.size()
      << " operating points for topK=" << R << " (recall "
      << table.points.front().recall << ".." << bestRecall << ", minRecall "
      << minRecall << ", took " << elapsedSecs()
      << "s). Operating points: " << formatOperatingPoints(ops, nullptr);
  return table;
}

}  // namespace arangodb::vector

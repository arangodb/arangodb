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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <format>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "Basics/ResourceUsage.h"
#include "Basics/ScopeGuard.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

#include <faiss/AutoTune.h>
#include <faiss/IndexHNSW.h>
#include <faiss/IndexIVF.h>
#include <faiss/IndexIVFPQ.h>
#include <faiss/impl/FaissException.h>

namespace arangodb::vector {

namespace {

// quantizer_params is non-owning, so these carry its storage inline.
struct OwningIVFSearchParameters : faiss::SearchParametersIVF {
  faiss::SearchParametersHNSW ownedQuantizerParams;
};
struct OwningIVFPQSearchParameters : faiss::IVFPQSearchParameters {
  faiss::SearchParametersHNSW ownedQuantizerParams;
};

// Recall capped at the target so the sweep's Pareto pruning treats every
// above-target operating point as non-improving and skips its search.
struct CappedIntersectionCriterion : faiss::IntersectionCriterion {
  double cap;
  CappedIntersectionCriterion(faiss::idx_t nq, faiss::idx_t R, double cap)
      : faiss::IntersectionCriterion(nq, R), cap(cap) {}
  double evaluate(float const* D, faiss::idx_t const* I) const override {
    return std::min(faiss::IntersectionCriterion::evaluate(D, I), cap);
  }
};

// Use nLists as nProbe to get ground truth. Distance is unused
ResultT<std::vector<faiss::idx_t>> computeGroundTruth(
    faiss::IndexIVF& index, std::span<float const> querySet,
    faiss::idx_t numberOfQueries, std::uint64_t R, void* invertedListContext,
    std::string_view logContext) {
  auto const total = static_cast<std::size_t>(numberOfQueries) * R;
  std::vector<faiss::idx_t> ids(total);
  std::vector<float> distancesScratch(total);
  faiss::SearchParametersIVF params;
  params.inverted_list_context = invertedListContext;
  params.nprobe = index.nlist;
  // nprobe=nlist is exhaustive only if the coarse quantizer returns every
  // centroid. An HNSW quantizer caps that at its efSearch, so raise efSearch to
  // nlist; otherwise the ground truth scans only a fraction of the lists.
  faiss::SearchParametersHNSW quantizerParams;
  if (dynamic_cast<faiss::IndexHNSW const*>(index.quantizer) != nullptr) {
    quantizerParams.efSearch = static_cast<int>(index.nlist);
    params.quantizer_params = &quantizerParams;
  }
  auto const start = std::chrono::steady_clock::now();
  try {
    index.search(numberOfQueries, querySet.data(), static_cast<faiss::idx_t>(R),
                 distancesScratch.data(), ids.data(), &params);
  } catch (faiss::FaissException const& e) {
    return Result{
        TRI_ERROR_INTERNAL,
        std::format("autotune ground-truth search failed: {}", e.what())};
  }
  LOG_TOPIC("e16b1", INFO, Logger::ENGINES)
      << logContext << "Autotune ground truth (nprobe=" << index.nlist
      << ") done in "
      << std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
             .count()
      << "s.";
  return ids;
}

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
  // Visit every combination, but via the path that consults the Pareto bounds
  // and skips the search for combinations that cannot beat the front. Paired
  // with the target-capped criterion this prunes the over-target tail.
  // (n_experiments==0 would search every combination unconditionally; the >0
  // path requires n_experiments > 2.)
  auto const numCombinations = ps.n_combinations();
  ps.n_experiments =
      numCombinations <= 2 ? 0 : static_cast<int>(numCombinations);
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

// This just flips the Wilson score interval half-width formula around to solve
// for n. See
// https://en.wikipedia.org/wiki/Binomial_proportion_confidence_interval#Wilson_score_interval
std::size_t wilsonSampleSize(double p, double z, double m) {
  TRI_ASSERT(p > 0.0 && p <= 1.0);
  TRI_ASSERT(z > 0.0);
  TRI_ASSERT(m > 0.0);

  // Wilson half-width == m solved for n: quadratic a n^2 + b n + c = 0, larger
  // root (c < 0 since m^2 < 1/4, so the discriminant is positive).
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
                                           ResourceMonitor& resourceMonitor,
                                           void* invertedListContext,
                                           std::uint64_t R, double targetRecall,
                                           std::string_view logContext) {
  TRI_ASSERT(R >= 1);
  TRI_ASSERT(targetRecall > 0.0 && targetRecall <= 1.0);
  TRI_ASSERT(index.d > 0);

  auto const d = static_cast<std::size_t>(index.d);
  TRI_ASSERT(querySet.size() > 0 && querySet.size() % d == 0)
      << "autotune query set size is not a positive multiple of the index "
         "dimension";
  auto const numberOfQueries = static_cast<faiss::idx_t>(querySet.size() / d);

  // computeGroundTruth holds R ids plus a same-sized distance scratch per
  // query; reserve both up front so an oversized run is rejected before it
  // allocates.
  ResourceUsageScope groundTruthScope(
      resourceMonitor, static_cast<std::uint64_t>(numberOfQueries) * R *
                           (sizeof(faiss::idx_t) + sizeof(float)));

  LOG_TOPIC("e16af", INFO, Logger::ENGINES)
      << logContext << "Autotune starting: numberOfQueries=" << numberOfQueries
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
        << logContext << "Autotune failed after " << elapsedSecs()
        << "s: " << outcome.errorMessage();
  });

  auto const gtStart = std::chrono::steady_clock::now();
  auto gt = computeGroundTruth(index, querySet, numberOfQueries, R,
                               invertedListContext, logContext);
  if (gt.fail()) {
    outcome = std::move(gt).result();
    return outcome;
  }
  double const groundTruthSecs =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - gtStart)
          .count();

  CappedIntersectionCriterion crit(numberOfQueries,
                                   static_cast<faiss::idx_t>(R), targetRecall);
  crit.set_groundtruth(static_cast<int>(R), nullptr, gt.get().data());

  auto opsRes = exploreParameterSpace(index, querySet, numberOfQueries,
                                      invertedListContext, crit);
  if (opsRes.fail()) {
    outcome = std::move(opsRes).result();
    return outcome;
  }
  auto const& ops = opsRes.get();

  // FAISS orders optimal_pts by ascending recall; skip the empty-keyed dummy.
  OperatingPointTable table;
  table.topK = R;
  table.targetRecall = targetRecall;
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
  if (bestRecall < targetRecall - kAutoTuneRecallEpsilon) {
    LOG_TOPIC("e16ac", WARN, Logger::ENGINES)
        << logContext
        << "Autotune could not reach targetRecall=" << targetRecall
        << " for topK=" << R << "; best attainable recall=" << bestRecall
        << ". Returning best-attainable table.";
  }
  double const totalSecs = elapsedSecs();
  LOG_TOPIC("e16ad", INFO, Logger::ENGINES)
      << logContext << "Autotune produced " << table.points.size()
      << " operating points for topK=" << R << " (recall "
      << table.points.front().recall << ".." << bestRecall << ", targetRecall "
      << targetRecall << ", took " << totalSecs << "s = ground truth "
      << groundTruthSecs << "s + sweep " << (totalSecs - groundTruthSecs)
      << "s). Operating points: " << formatOperatingPoints(ops, nullptr);
  return table;
}

ResultT<OperatingPoint> selectOperatingPoint(TunedTables const& tables,
                                             std::uint64_t topK,
                                             double targetRecall) {
  auto const it = tables.find(topK);
  if (it == tables.end()) {
    return Result{
        TRI_ERROR_BAD_PARAMETER,
        std::format("no autotuned operating-point table for topK={}; run "
                    "autotune for this topK first",
                    topK)};
  }

  // ascending recall (== ascending cost): first match is the cheapest.
  for (auto const& point : it->second.points) {
    if (point.recall >= targetRecall - kAutoTuneRecallEpsilon) {
      return point;
    }
  }

  double const best =
      it->second.points.empty() ? 0.0 : it->second.points.back().recall;
  return Result{
      TRI_ERROR_BAD_PARAMETER,
      std::format("targetRecall={} exceeds the autotuned range for topK={} "
                  "(highest achievable recall {})",
                  targetRecall, topK, best)};
}

// FAISS produce a key of search params like so: "nprobe=8,ht=20,max_codes=12".
// This parses that and builds SearchParametersIVF with the
ResultT<std::unique_ptr<faiss::SearchParametersIVF>>
makeSearchParametersFromKey(faiss::IndexIVF const& index,
                            std::string const& key) {
  bool const isPQ = dynamic_cast<faiss::IndexIVFPQ const*>(&index) != nullptr;
  bool const hasHnswQuantizer =
      dynamic_cast<faiss::IndexHNSW const*>(index.quantizer) != nullptr;

  std::unique_ptr<faiss::SearchParametersIVF> params;
  faiss::SearchParametersHNSW* ownedQuantizer = nullptr;
  if (isPQ) {
    auto p = std::make_unique<OwningIVFPQSearchParameters>();
    ownedQuantizer = &p->ownedQuantizerParams;
    params = std::move(p);
  } else {
    auto p = std::make_unique<OwningIVFSearchParameters>();
    ownedQuantizer = &p->ownedQuantizerParams;
    params = std::move(p);
  }

  // Comma-separated `name=value` tokens, e.g. "nprobe=8,ht=20".
  const auto getPair = [](std::string_view token)
      -> std::pair<std::string_view, std::string_view> {
    auto const eq = std::ranges::find(token, '=');
    if (eq == token.end()) {
      return {};
    }
    return {std::string_view(token.begin(), eq),
            std::string_view(eq + 1, token.end())};
  };
  for (auto const token : std::views::split(key, ',')) {
    std::string_view const tokenView(token.begin(), token.end());
    auto const [name, value] = getPair(tokenView);
    try {
      if (name == "nprobe") {
        params->nprobe = std::stoull(std::string(value));
      } else if (name == "max_codes") {
        params->max_codes = std::stoull(std::string(value));
      } else if (name == "ht") {
        if (!isPQ) {
          return Result{
              TRI_ERROR_INTERNAL,
              std::format("autotune parameter 'ht' requires a PQ index")};
        }
        static_cast<faiss::IVFPQSearchParameters*>(params.get())
            ->polysemous_ht = static_cast<int>(std::stoll(std::string(value)));
      } else if (name == "quantizer_efSearch") {
        if (!hasHnswQuantizer) {
          return Result{TRI_ERROR_INTERNAL,
                        std::format("autotune parameter 'quantizer_efSearch' "
                                    "requires an HNSW coarse quantizer")};
        }
        ownedQuantizer->efSearch =
            static_cast<int>(std::stoll(std::string(value)));
        params->quantizer_params = ownedQuantizer;
      } else {
        return Result{TRI_ERROR_NOT_IMPLEMENTED,
                      std::format("unsupported autotune parameter '{}'", name)};
      }
    } catch (std::exception const&) {
      return Result{
          TRI_ERROR_INTERNAL,
          std::format("could not parse autotune token '{}'", tokenView)};
    }
  }

  return params;
}

}  // namespace arangodb::vector

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
                                           std::int64_t R, double minRecall) {
  TRI_ASSERT(R >= 1);
  TRI_ASSERT(minRecall > 0.0 && minRecall <= 1.0);
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

  // FAISS orders optimal_pts by ascending recall; skip the empty-keyed dummy.
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

ResultT<OperatingPoint> selectOperatingPoint(
    std::vector<OperatingPointTable> const& tables, std::int64_t topK,
    double targetRecall) {
  auto const it = std::ranges::find(tables, topK, &OperatingPointTable::topK);
  if (it == tables.end()) {
    return Result{
        TRI_ERROR_BAD_PARAMETER,
        std::format("no autotuned operating-point table for topK={}; run "
                    "autotune for this topK first",
                    topK)};
  }
  // ascending recall (== ascending cost): first match is the cheapest.
  for (auto const& point : it->points) {
    if (point.recall >= targetRecall - kAutoTuneRecallEpsilon) {
      return point;
    }
  }
  double const best = it->points.empty() ? 0.0 : it->points.back().recall;
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

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

#include <cstddef>
#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"

#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFFlat.h>
#include <faiss/MetricType.h>

using namespace arangodb::vector;

namespace {

// Build a clustered synthetic dataset: `nClusters` Gaussian blobs in R^d,
// each blob has `pointsPerCluster` vectors. nprobe=1 routinely misses the
// tail neighbors of a query because the IVF coarse quantizer rarely places
// all R true neighbors in the same Voronoi cell as the query when blobs
// overlap. Larger nprobe sweeps the neighboring cells.
struct ClusterDataset {
  std::vector<float> vectors;  // (nClusters * pointsPerCluster) * d
  std::size_t d;
  std::size_t n;
};

ClusterDataset makeClusterDataset(std::size_t d, std::size_t nClusters,
                                  std::size_t pointsPerCluster,
                                  std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::uniform_real_distribution<float> centerDist(-5.0f, 5.0f);
  std::normal_distribution<float> jitter(0.0f, 1.0f);

  std::vector<float> centers(nClusters * d);
  for (auto& c : centers) {
    c = centerDist(rng);
  }

  ClusterDataset ds;
  ds.d = d;
  ds.n = nClusters * pointsPerCluster;
  ds.vectors.reserve(ds.n * d);
  for (std::size_t c = 0; c < nClusters; ++c) {
    for (std::size_t p = 0; p < pointsPerCluster; ++p) {
      for (std::size_t k = 0; k < d; ++k) {
        ds.vectors.push_back(centers[c * d + k] + jitter(rng));
      }
    }
  }
  return ds;
}

// Train and populate an IVFFlat index over `ds`, with `nlist` coarse
// centroids. The returned index has nprobe = 1 as a starting point.
std::pair<std::unique_ptr<faiss::IndexFlatL2>,
          std::unique_ptr<faiss::IndexIVFFlat>>
buildIvfIndex(ClusterDataset const& ds, std::size_t nlist) {
  auto quantizer = std::make_unique<faiss::IndexFlatL2>(ds.d);
  auto ivf = std::make_unique<faiss::IndexIVFFlat>(quantizer.get(), ds.d, nlist,
                                                   faiss::METRIC_L2);
  ivf->train(static_cast<faiss::idx_t>(ds.n), ds.vectors.data());
  ivf->add(static_cast<faiss::idx_t>(ds.n), ds.vectors.data());
  ivf->nprobe = 1;
  return {std::move(quantizer), std::move(ivf)};
}

// Compute recall@R for the current `index.nprobe` setting, using `xq` as
// queries against ground truth `gtI` (size nq * R, exhaustive top-R).
double recallAtR(faiss::IndexIVFFlat& index, std::vector<float> const& xq,
                 std::vector<faiss::idx_t> const& gtI, std::int64_t R) {
  auto const nq = static_cast<faiss::idx_t>(xq.size() / index.d);
  std::vector<faiss::idx_t> I(nq * R);
  std::vector<float> D(nq * R);
  index.search(nq, xq.data(), R, D.data(), I.data());

  std::size_t hits = 0;
  for (faiss::idx_t q = 0; q < nq; ++q) {
    std::unordered_set<faiss::idx_t> truth(gtI.begin() + q * R,
                                           gtI.begin() + (q + 1) * R);
    for (std::int64_t i = 0; i < R; ++i) {
      if (truth.contains(I[q * R + i])) {
        ++hits;
      }
    }
  }
  return static_cast<double>(hits) / static_cast<double>(nq * R);
}

}  // namespace

// Sanity: a baseline of nprobe=1 on a clustered dataset has low recall,
// proving the dataset is non-trivial and tuning has something to do.
TEST(VectorIndexAutoTunerTest, baselineNProbeOneHasLowRecall) {
  constexpr std::size_t d = 16;
  constexpr std::size_t nClusters = 32;
  constexpr std::size_t pointsPerCluster = 200;
  constexpr std::size_t nlist = 64;
  constexpr std::int64_t R = 10;

  auto ds = makeClusterDataset(d, nClusters, pointsPerCluster, /*seed=*/42);
  auto [quantizer, ivf] = buildIvfIndex(ds, nlist);

  // Query set: first 256 indexed vectors (also our gt source).
  constexpr std::size_t nq = 256;
  std::vector<float> xq(ds.vectors.begin(), ds.vectors.begin() + nq * d);

  // Ground truth via exhaustive IVF (nprobe = nlist).
  std::vector<faiss::idx_t> gtI(nq * R);
  std::vector<float> gtD(nq * R);
  ivf->nprobe = nlist;
  ivf->search(nq, xq.data(), R, gtD.data(), gtI.data());

  ivf->nprobe = 1;
  auto const recall = recallAtR(*ivf, xq, gtI, R);
  // Empirically well below 0.9 for this dataset; assert a generous bound so
  // the test isn't fragile to small ParameterSpace tweaks upstream.
  EXPECT_LT(recall, 0.85);
}

TEST(VectorIndexAutoTunerTest, picksNProbeAboveOneAndMeetsTargetRecall) {
  constexpr std::size_t d = 16;
  constexpr std::size_t nClusters = 32;
  constexpr std::size_t pointsPerCluster = 200;
  constexpr std::size_t nlist = 64;
  constexpr std::int64_t R = 10;
  constexpr double target = 0.9;

  auto ds = makeClusterDataset(d, nClusters, pointsPerCluster, /*seed=*/42);
  auto [quantizer, ivf] = buildIvfIndex(ds, nlist);

  constexpr std::size_t nq = 256;
  std::vector<float> xq(ds.vectors.begin(), ds.vectors.begin() + nq * d);

  auto tuned =
      autoTuneNProbe(*ivf, xq, /*invertedListContext=*/nullptr, R, target);
  ASSERT_TRUE(tuned.ok()) << tuned.errorMessage();
  EXPECT_GT(tuned.get(), 1) << "autotune should pick nprobe > 1 on a "
                               "clustered dataset where nprobe=1 underperforms";
  EXPECT_LE(tuned.get(), static_cast<std::int64_t>(nlist));

  // Recompute recall at the chosen nprobe against the same ground truth as
  // the tuner used and verify the criterion is met.
  std::vector<faiss::idx_t> gtI(nq * R);
  std::vector<float> gtD(nq * R);
  auto const savedNProbe = ivf->nprobe;
  ivf->nprobe = nlist;
  ivf->search(nq, xq.data(), R, gtD.data(), gtI.data());
  ivf->nprobe = savedNProbe;  // autotuner already set it; keep the chosen one

  EXPECT_GE(recallAtR(*ivf, xq, gtI, R), target);
}

TEST(VectorIndexAutoTunerTest, misalignedQuerySetTrapsAssertion) {
  constexpr std::size_t d = 16;
  auto ds = makeClusterDataset(d, /*nClusters=*/8, /*pointsPerCluster=*/64,
                               /*seed=*/7);
  auto [quantizer, ivf] = buildIvfIndex(ds, /*nlist=*/8);

  // querySet length is not a multiple of d: precondition violation, the
  // TRI_ASSERT in autoTuneNProbe should fire and abort the process.
  std::vector<float> bad(d + 1);
  EXPECT_DEATH(autoTuneNProbe(*ivf, bad, /*invertedListContext=*/nullptr,
                              /*R=*/10, /*targetRecall=*/0.9),
               "");
}

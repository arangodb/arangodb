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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <unordered_set>
#include <vector>

#include "gtest/gtest.h"

#include "Mocks/Death_Test.h"

#include <faiss/IndexFlat.h>
#include <faiss/IndexHNSW.h>
#include <faiss/IndexIVFFlat.h>
#include <faiss/IndexIVFPQ.h>
#include <faiss/MetricType.h>
#include <faiss/index_factory.h>

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

// IVFPQ index; its sweep adds `ht`. `np` skips slow polysemous training.
std::shared_ptr<faiss::IndexIVFPQ> buildIvfpqIndex(ClusterDataset const& ds,
                                                   char const* factory) {
  std::shared_ptr<faiss::Index> index(
      faiss::index_factory(static_cast<int>(ds.d), factory, faiss::METRIC_L2));
  auto ivfpq = std::dynamic_pointer_cast<faiss::IndexIVFPQ>(index);
  ivfpq->train(static_cast<faiss::idx_t>(ds.n), ds.vectors.data());
  ivfpq->add(static_cast<faiss::idx_t>(ds.n), ds.vectors.data());
  ivfpq->nprobe = 1;
  return ivfpq;
}

// IVF index with an HNSW coarse quantizer; its sweep adds quantizer_efSearch.
std::shared_ptr<faiss::IndexIVF> buildIvfHnswIndex(ClusterDataset const& ds,
                                                   char const* factory) {
  std::shared_ptr<faiss::Index> index(
      faiss::index_factory(static_cast<int>(ds.d), factory, faiss::METRIC_L2));
  auto ivf = std::dynamic_pointer_cast<faiss::IndexIVF>(index);
  ivf->train(static_cast<faiss::idx_t>(ds.n), ds.vectors.data());
  ivf->add(static_cast<faiss::idx_t>(ds.n), ds.vectors.data());
  ivf->nprobe = 1;
  return ivf;
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
TEST(AutoTunerTest, baselineNProbeOneHasLowRecall) {
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

TEST(AutoTunerTest, producesAscendingTableReachingMinRecall) {
  constexpr std::size_t d = 16;
  constexpr std::size_t nClusters = 32;
  constexpr std::size_t pointsPerCluster = 200;
  constexpr std::size_t nlist = 64;
  constexpr std::int64_t R = 10;
  constexpr double minRecall = 0.9;

  auto ds = makeClusterDataset(d, nClusters, pointsPerCluster, /*seed=*/42);
  auto [quantizer, ivf] = buildIvfIndex(ds, nlist);

  constexpr std::size_t nq = 256;
  std::vector<float> xq(ds.vectors.begin(), ds.vectors.begin() + nq * d);

  auto tuned =
      autoTuneTable(*ivf, xq, /*invertedListContext=*/nullptr, R, minRecall);
  ASSERT_TRUE(tuned.ok()) << tuned.errorMessage();
  auto const& table = tuned.get();

  EXPECT_EQ(table.topK, R);
  EXPECT_DOUBLE_EQ(table.minRecall, minRecall);
  ASSERT_FALSE(table.points.empty());

  // The Pareto front is ordered by ascending recall (== ascending cost).
  for (std::size_t i = 1; i < table.points.size(); ++i) {
    EXPECT_GE(table.points[i].recall, table.points[i - 1].recall);
  }
  // On this clustered dataset the sweep can reach the requested recall.
  EXPECT_GE(table.points.back().recall, minRecall);
  // The keys are the verbatim FAISS combination strings (nprobe sweep here).
  EXPECT_NE(table.points.front().faissKey.find("nprobe="), std::string::npos);
}

TEST(AutoTunerTest, pqSweepEmitsOnlyApplicableParameters) {
  constexpr std::size_t d = 16;
  constexpr std::size_t nClusters = 16;
  constexpr std::size_t pointsPerCluster = 64;
  constexpr std::int64_t R = 10;
  constexpr double minRecall = 0.9;

  auto ds = makeClusterDataset(d, nClusters, pointsPerCluster, /*seed=*/42);
  auto ivfpq = buildIvfpqIndex(ds, "IVF16,PQ8np");

  constexpr std::size_t nq = 128;
  std::vector<float> xq(ds.vectors.begin(), ds.vectors.begin() + nq * d);

  auto tuned =
      autoTuneTable(*ivfpq, xq, /*invertedListContext=*/nullptr, R, minRecall);
  ASSERT_TRUE(tuned.ok()) << tuned.errorMessage();
  ASSERT_FALSE(tuned.get().points.empty());

  // Every key token must be an applicable parameter.
  for (auto const& point : tuned.get().points) {
    std::string const& key = point.faissKey;
    for (std::size_t pos = 0; pos < key.size();) {
      auto const comma = key.find(',', pos);
      std::string const token = key.substr(
          pos, comma == std::string::npos ? std::string::npos : comma - pos);
      pos = comma == std::string::npos ? key.size() : comma + 1;
      std::string const name = token.substr(0, token.find('='));
      EXPECT_TRUE(name == "nprobe" || name == "max_codes" || name == "ht" ||
                  name == "quantizer_efSearch")
          << "unexpected tuned parameter '" << name << "' in key '" << key
          << "'";
    }
  }
}

TEST(AutoTunerTest, makeSearchParametersFromKeyParsesIvfFields) {
  auto ds = makeClusterDataset(/*d=*/16, /*nClusters=*/8,
                               /*pointsPerCluster=*/64, /*seed=*/7);
  auto [quantizer, ivf] = buildIvfIndex(ds, /*nlist=*/8);

  auto params = makeSearchParametersFromKey(*ivf, "nprobe=8,max_codes=100");
  ASSERT_TRUE(params.ok()) << params.errorMessage();
  EXPECT_EQ(params.get()->nprobe, 8U);
  EXPECT_EQ(params.get()->max_codes, 100U);
  EXPECT_EQ(dynamic_cast<faiss::IVFPQSearchParameters*>(params.get().get()),
            nullptr);
}

TEST(AutoTunerTest, makeSearchParametersFromKeyAppliesHtForPq) {
  auto ds = makeClusterDataset(/*d=*/16, /*nClusters=*/16,
                               /*pointsPerCluster=*/64, /*seed=*/42);
  auto ivfpq = buildIvfpqIndex(ds, "IVF16,PQ8np");

  auto params = makeSearchParametersFromKey(*ivfpq, "nprobe=4,ht=20");
  ASSERT_TRUE(params.ok()) << params.errorMessage();
  EXPECT_EQ(params.get()->nprobe, 4U);
  auto* pqParams =
      dynamic_cast<faiss::IVFPQSearchParameters*>(params.get().get());
  ASSERT_NE(pqParams, nullptr);
  EXPECT_EQ(pqParams->polysemous_ht, 20);
}

TEST(AutoTunerTest, makeSearchParametersFromKeyAppliesQuantizerEfSearch) {
  auto ds = makeClusterDataset(/*d=*/16, /*nClusters=*/16,
                               /*pointsPerCluster=*/64, /*seed=*/42);
  auto ivf = buildIvfHnswIndex(ds, "IVF16_HNSW32,Flat");

  auto params =
      makeSearchParametersFromKey(*ivf, "nprobe=4,quantizer_efSearch=40");
  ASSERT_TRUE(params.ok()) << params.errorMessage();
  EXPECT_EQ(params.get()->nprobe, 4U);
  ASSERT_NE(params.get()->quantizer_params, nullptr);
  auto* hnswParams = dynamic_cast<faiss::SearchParametersHNSW*>(
      params.get()->quantizer_params);
  ASSERT_NE(hnswParams, nullptr);
  EXPECT_EQ(hnswParams->efSearch, 40);
}

TEST(AutoTunerTest, makeSearchParametersFromKeyRejectsInapplicableKeys) {
  auto ds = makeClusterDataset(/*d=*/16, /*nClusters=*/8,
                               /*pointsPerCluster=*/64, /*seed=*/7);
  auto [quantizer, ivf] = buildIvfIndex(ds, /*nlist=*/8);

  EXPECT_TRUE(makeSearchParametersFromKey(*ivf, "nprobe=4,ht=20").fail());
  EXPECT_TRUE(
      makeSearchParametersFromKey(*ivf, "quantizer_efSearch=16").fail());
  EXPECT_TRUE(makeSearchParametersFromKey(*ivf, "nprobe").fail());
}

TEST(AutoTunerTest, misalignedQuerySetTrapsAssertion) {
  constexpr std::size_t d = 16;
  auto ds = makeClusterDataset(d, /*nClusters=*/8, /*pointsPerCluster=*/64,
                               /*seed=*/7);
  auto [quantizer, ivf] = buildIvfIndex(ds, /*nlist=*/8);

  // querySet length is not a multiple of d: precondition violation, the
  // TRI_ASSERT in autoTuneTable should fire and abort the process.
  std::vector<float> bad(d + 1);
  EXPECT_DEATH_CORE_FREE(
      autoTuneTable(*ivf, bad, /*invertedListContext=*/nullptr,
                    /*R=*/10, /*minRecall=*/0.9),
      "");
}

// The worked examples from the proposal (confidence 0.99 -> z = 2.576,
// recallTolerance 0.03). Tolerance of 1 absorbs the z rounding in the doc.
TEST(AutoTunerTest, wilsonSampleSizeMatchesWorkedExamples) {
  constexpr double z = kAutoTuneConfidenceZ;
  constexpr double m = kAutoTuneRecallTolerance;
  EXPECT_NEAR(static_cast<double>(wilsonSampleSize(0.90, z, m)), 668.0, 1.0);
  EXPECT_NEAR(static_cast<double>(wilsonSampleSize(0.80, z, m)), 1177.0, 1.0);
  EXPECT_NEAR(static_cast<double>(wilsonSampleSize(0.50, z, m)), 1837.0, 1.0);
}

// p = 0.5 is the maximum-variance point: it must demand the largest sample.
TEST(AutoTunerTest, wilsonSampleSizePeaksAtHalf) {
  constexpr double z = kAutoTuneConfidenceZ;
  constexpr double m = kAutoTuneRecallTolerance;
  EXPECT_GT(wilsonSampleSize(0.50, z, m), wilsonSampleSize(0.80, z, m));
  EXPECT_GT(wilsonSampleSize(0.80, z, m), wilsonSampleSize(0.90, z, m));
  EXPECT_GT(wilsonSampleSize(0.90, z, m), wilsonSampleSize(0.99, z, m));
}

// Tighter tolerance and higher confidence both enlarge the required sample.
TEST(AutoTunerTest, wilsonSampleSizeGrowsWithPrecisionAndConfidence) {
  constexpr double m = kAutoTuneRecallTolerance;
  EXPECT_GT(wilsonSampleSize(0.90, kAutoTuneConfidenceZ, 0.01),
            wilsonSampleSize(0.90, kAutoTuneConfidenceZ, m));
  // z: 0.95 -> 1.960, 0.99 -> 2.576.
  EXPECT_GT(wilsonSampleSize(0.90, kAutoTuneConfidenceZ, m),
            wilsonSampleSize(0.90, 1.959963985, m));
}

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
////////////////////////////////////////////////////////////////////////////////

#include "VectorIndex/ListAssignment.h"

#include "Assertions/Assert.h"

#include <faiss/Index.h>
#include <faiss/IndexIVF.h>
#include <faiss/utils/distances.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <tuple>
#include <vector>

namespace arangodb::vector {
namespace {

// Nearest centroids the SOAR secondary is chosen from.
constexpr std::size_t kNumCandidates = 32;

// The quantizer's candidates re-ranked by exact distance, nearest first. The
// quantizer ranks a batch through BLAS but a single vector without it, so
// near-ties could otherwise send an insert and its remove to different lists.
struct Candidates {
  std::size_t dim;
  std::vector<faiss::idx_t> listIds;
  // Smaller is closer, also for inner product
  std::vector<float> distances;
  std::vector<float> centroids;

  std::size_t size() const noexcept { return listIds.size(); }
  float const* centroid(std::size_t j) const noexcept {
    return centroids.data() + j * dim;
  }
};

Candidates rankCandidates(faiss::Index const& quantizer, float const* x,
                          std::span<faiss::idx_t const> labels) {
  auto const dim = static_cast<std::size_t>(quantizer.d);
  auto const valid = labels.subspan(
      0, std::ranges::find_if(labels, [](auto id) { return id < 0; }) -
             labels.begin());
  TRI_ASSERT(!valid.empty());

  std::vector<float> centroids(valid.size() * dim);
  std::vector<float> distances(valid.size());
  bool const similarity = faiss::is_similarity_metric(quantizer.metric_type);
  for (std::size_t j = 0; j < valid.size(); ++j) {
    auto* centroid = centroids.data() + j * dim;
    quantizer.reconstruct(valid[j], centroid);
    distances[j] = similarity ? -faiss::fvec_inner_product(x, centroid, dim)
                              : faiss::fvec_L2sqr(x, centroid, dim);
  }

  std::vector<std::size_t> order(valid.size());
  std::iota(order.begin(), order.end(), 0);
  std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
    return std::tie(distances[a], valid[a]) < std::tie(distances[b], valid[b]);
  });

  Candidates candidates{.dim = dim};
  candidates.centroids.resize(centroids.size());
  for (auto const j : order) {
    std::copy_n(centroids.data() + j * dim, dim,
                candidates.centroids.data() + candidates.size() * dim);
    candidates.listIds.push_back(valid[j]);
    candidates.distances.push_back(distances[j]);
  }
  return candidates;
}

bool isNearBoundary(Candidates const& candidates, float epsilon) {
  if (epsilon <= 0.0f) {
    return true;
  }
  double const primary = candidates.distances[0];
  double const second = candidates.distances[1];
  if (primary == 0.0) {
    return false;
  }
  return (second - primary) / std::fabs(primary) <= epsilon;
}

// Minimizes ||x - c||^2 + lambda * (r . (x - c))^2 with r the normalized
// residual of x to its primary centroid.
std::optional<faiss::idx_t> findSoarSecondary(Candidates const& candidates,
                                              float const* x, float lambda) {
  auto const dim = candidates.dim;
  std::vector<float> residual(dim);
  faiss::fvec_sub(dim, x, candidates.centroid(0), residual.data());
  if (auto const norm = faiss::fvec_norm_L2sqr(residual.data(), dim);
      norm > 1e-7f) {
    auto const inverse = 1.0f / std::sqrt(norm);
    std::ranges::transform(residual, residual.begin(),
                           [&](float value) { return value * inverse; });
  }
  auto const residualDotX = faiss::fvec_inner_product(residual.data(), x, dim);

  std::optional<faiss::idx_t> best;
  float bestLoss = std::numeric_limits<float>::max();
  for (std::size_t j = 1; j < candidates.size(); ++j) {
    auto const* centroid = candidates.centroid(j);
    auto const distance = faiss::fvec_L2sqr(x, centroid, dim);
    if (distance >= bestLoss) {
      continue;
    }
    auto const gap = residualDotX -
                     faiss::fvec_inner_product(residual.data(), centroid, dim);
    if (auto const loss = distance + lambda * gap * gap; loss < bestLoss) {
      bestLoss = loss;
      best = candidates.listIds[j];
    }
  }
  return best;
}

std::optional<faiss::idx_t> findSecondary(Candidates const& candidates,
                                          ListAssignmentConfig const& config,
                                          float const* x) {
  if (candidates.size() < 2 ||
      !isNearBoundary(candidates, config.boundaryEpsilon)) {
    return std::nullopt;
  }
  if (config.soarLambda > 0.0f) {
    return findSoarSecondary(candidates, x, config.soarLambda);
  }
  return candidates.listIds[1];
}

}  // namespace

ListAssignments assignToLists(faiss::IndexIVF const& index,
                              ListAssignmentConfig const& config, std::size_t n,
                              float const* x) {
  TRI_ASSERT(index.quantizer != nullptr);
  auto const& quantizer = *index.quantizer;
  auto const dim = static_cast<std::size_t>(index.d);

  bool const replicate =
      config.soarLambda > 0.0f || config.boundaryEpsilon > 0.0f;
  auto const k =
      replicate ? std::min(kNumCandidates, index.nlist) : std::size_t{1};

  std::vector<float> distances(n * k);
  std::vector<faiss::idx_t> labels(n * k);
  quantizer.search(static_cast<faiss::idx_t>(n), x,
                   static_cast<faiss::idx_t>(k), distances.data(),
                   labels.data());

  ListAssignments result;
  result.vectorIndices.reserve(replicate ? 2 * n : n);
  result.listIds.reserve(result.vectorIndices.capacity());
  for (std::size_t i = 0; i < n; ++i) {
    auto const vectorLabels =
        std::span<faiss::idx_t const>(labels).subspan(i * k, k);
    if (!replicate) {
      result.vectorIndices.push_back(i);
      result.listIds.push_back(vectorLabels[0]);
      continue;
    }

    // Primary centroid
    auto const* vector = x + i * dim;
    auto const candidates = rankCandidates(quantizer, vector, vectorLabels);
    result.vectorIndices.push_back(i);
    result.listIds.push_back(candidates.listIds[0]);

    // Secondary centroid
    if (auto const secondary = findSecondary(candidates, config, vector);
        secondary.has_value() && *secondary != candidates.listIds[0]) {
      result.vectorIndices.push_back(i);
      result.listIds.push_back(*secondary);
    }
  }
  return result;
}

}  // namespace arangodb::vector

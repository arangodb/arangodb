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

#pragma once

#include <faiss/MetricType.h>

#include <cstddef>
#include <vector>

namespace faiss {
struct IndexIVF;
}

namespace arangodb::vector {

// Both soarLambda and boundaryEpsilon at 0 disable the secondary list.
struct ListAssignmentConfig {
  // SOAR weight for picking the secondary; 0 picks the 2nd-nearest list.
  float soarLambda{1.0f};

  // Add a secondary only if (d2 - d1) / |d1| <= epsilon for the two nearest
  // centroid distances; 0 adds one to every vector.
  float boundaryEpsilon{0.35f};
};

// Vector vectorIndices[k] belongs to list listIds[k]. A vector has its
// primary (nearest centroid) list, optionally followed by one secondary.
struct ListAssignments {
  std::vector<std::size_t> vectorIndices;
  std::vector<faiss::idx_t> listIds;
};

// Each vector goes to its nearest centroid c_0. It gets a secondary only if
// d(x, c_1) <= (1 + boundaryEpsilon) * d(x, c_0)
// SOAR then picks among the 32 nearest centroids the secondary centroid
ListAssignments assignToLists(faiss::IndexIVF const& index,
                              ListAssignmentConfig const& config, std::size_t n,
                              float const* x);

}  // namespace arangodb::vector

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2026 ArangoDB GmbH, Hyderabad, India
/// Copyright 2026 triAGENS GmbH, Hyderabad, India
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
/// Copyright holder is ArangoDB GmbH, Hyderabad, India
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Basics/ResultT.h"
#include "VectorIndex/Definition.h"

#include <faiss/MetricType.h>

#include <cstddef>
#include <memory>

namespace faiss {
struct IndexIVF;
}

namespace arangodb::vector {

inline faiss::MetricType metricToFaissMetric(
    SimilarityMetric const metric) noexcept {
  switch (metric) {
    case SimilarityMetric::kL2:
      return faiss::MetricType::METRIC_L2;
    case SimilarityMetric::kCosine:
      return faiss::MetricType::METRIC_INNER_PRODUCT;
    case SimilarityMetric::kInnerProduct:
      return faiss::METRIC_INNER_PRODUCT;
  }
}

/// @brief Builds the IVF index described by def.factory with nLists
/// substituted into an "IVF{}" template. Fails if FAISS cannot parse the
/// string, if it does not describe an IVF index, or if its nlist differs
/// from nLists.
ResultT<std::shared_ptr<faiss::IndexIVF>> createIvfIndexFromFactory(
    UserDefinition const& def, std::size_t nLists);

/// @brief Validate the FAISS factory string
Result validateFactoryString(UserDefinition const& def);

}  // namespace arangodb::vector

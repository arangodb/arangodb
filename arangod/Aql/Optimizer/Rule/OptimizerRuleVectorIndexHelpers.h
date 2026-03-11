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

#pragma once

#include "Assertions/Assert.h"
#include "Indexes/Index.h"
#include "Indexes/VectorIndexDefinition.h"

#include <memory>
#include <string_view>
#include <vector>

namespace arangodb::aql {

inline bool checkFunctionNameMatchesIndexMetric(
    std::string_view functionName,
    UserVectorIndexDefinition const& definition) {
  switch (definition.metric) {
    case SimilarityMetric::kL2:
      return functionName == "APPROX_NEAR_L2";
    case SimilarityMetric::kCosine:
      return functionName == "APPROX_NEAR_COSINE";
    case SimilarityMetric::kInnerProduct:
      return functionName == "APPROX_NEAR_INNER_PRODUCT";
  }
  return false;
}

inline bool checkAscendingMatchesMetric(
    std::shared_ptr<Index> const& vectorIndex, bool ascending) {
  switch (vectorIndex->getVectorIndexDefinition().metric) {
    case SimilarityMetric::kL2:
      return ascending;
    case SimilarityMetric::kCosine:
    case SimilarityMetric::kInnerProduct:
      return !ascending;
  }
  return false;
}

inline bool isIndexedFieldSameAsSearched(
    std::shared_ptr<Index> const& vectorIndex,
    std::vector<basics::AttributeName> const& attributeName) {
  TRI_ASSERT(vectorIndex->fields().size() == 1);
  return attributeName == vectorIndex->fields()[0];
}

inline bool isCompatibleVectorIndex(std::shared_ptr<Index> const& candidate,
                                    std::shared_ptr<Index> const& currentIndex,
                                    bool ascending) {
  if (candidate->type() != Index::IndexType::TRI_IDX_TYPE_VECTOR_INDEX) {
    return false;
  }
  if (candidate->getVectorIndexDefinition().metric !=
      currentIndex->getVectorIndexDefinition().metric) {
    return false;
  }
  if (!checkAscendingMatchesMetric(candidate, ascending)) {
    return false;
  }
  TRI_ASSERT(candidate->fields().size() == 1);
  TRI_ASSERT(currentIndex->fields().size() == 1);
  if (candidate->fields()[0] != currentIndex->fields()[0]) {
    return false;
  }
  return true;
}

}  // namespace arangodb::aql

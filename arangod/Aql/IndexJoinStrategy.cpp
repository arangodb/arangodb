////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
#include <memory>

#include <velocypack/Slice.h>
#include "Basics/VelocyPackHelper.h"

#include "IndexJoinStrategy.h"
#include "Aql/IndexJoin/GenericMerge.h"
#include "Aql/IndexJoin/TwoIndicesMergeJoin.h"
#include "Aql/IndexJoin/TwoIndicesUniqueMergeJoin.h"
#include "Aql/QueryOptions.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

using namespace arangodb::aql;

namespace {
struct VPackSliceComparator {
  auto operator()(VPackSlice left, VPackSlice right) {
    return arangodb::basics::VelocyPackHelper::compare(left, right, true) <=> 0;
  }
};
}  // namespace
auto IndexJoinStrategyFactory::createStrategy(
    std::vector<Descriptor> desc, std::size_t numKeyComponents,
    aql::QueryOptions::JoinStrategyType joinStrategy)
    -> std::unique_ptr<AqlIndexJoinStrategy> {
  if (desc.size() == 2 &&
      joinStrategy != aql::QueryOptions::JoinStrategyType::GENERIC) {
    if (desc[0].isUnique && desc[1].isUnique) {
      // build optimized merge join strategy for two unique indices
      return std::make_unique<TwoIndicesUniqueMergeJoin<
          velocypack::Slice, LocalDocumentId, VPackSliceComparator>>(
          std::move(desc), numKeyComponents);
    } else {
      // build optimized merge join strategy for two non-unique indices
      return std::make_unique<TwoIndicesMergeJoin<
          velocypack::Slice, LocalDocumentId, VPackSliceComparator>>(
          std::move(desc), numKeyComponents);
    }
  }
  return std::make_unique<GenericMergeJoin<velocypack::Slice, LocalDocumentId,
                                           VPackSliceComparator>>(
      std::move(desc), numKeyComponents);
}

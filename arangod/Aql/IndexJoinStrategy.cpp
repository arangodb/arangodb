////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "IndexJoinStrategy.h"

#include "Aql/IndexJoin/GenericMerge.h"
#include "Aql/IndexJoin/TwoIndicesMergeJoin.h"
#include "Aql/IndexJoin/TwoIndicesUniqueMergeJoin.h"
#include "Aql/QueryOptions.h"
#include "Basics/VelocyPackHelper.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

#include <velocypack/Slice.h>

#include <memory>

namespace arangodb::aql {

namespace {
template<IndexJoinKeyOrder order>
struct VPackSliceComparator {
  auto operator()(VPackSlice left, VPackSlice right) const {
    if constexpr (order == IndexJoinKeyOrder::kBinary) {
      return basics::VelocyPackHelper::compare(left, right, false) <=> 0;
    } else if constexpr (order == IndexJoinKeyOrder::kVPackLegacy) {
      return basics::VelocyPackHelper::compareLegacy(left, right, true) <=> 0;
    } else {
      return basics::VelocyPackHelper::compare(left, right, true) <=> 0;
    }
  }
};

template<typename Comparator>
auto createStrategyWithComparator(
    std::vector<IndexJoinStrategyFactory::Descriptor> desc,
    aql::QueryOptions::JoinStrategyType joinStrategy)
    -> std::unique_ptr<AqlIndexJoinStrategy> {
  if (desc.size() == 2 &&
      joinStrategy != aql::QueryOptions::JoinStrategyType::kGeneric) {
    if (desc[0].isUniqueStream && desc[1].isUniqueStream) {
      // build optimized merge join strategy for two unique indices
      return std::make_unique<TwoIndicesUniqueMergeJoin<
          velocypack::Slice, LocalDocumentId, Comparator>>(std::move(desc));
    } else {
      // build optimized merge join strategy for two non-unique indices
      return std::make_unique<
          TwoIndicesMergeJoin<velocypack::Slice, LocalDocumentId, Comparator>>(
          std::move(desc));
    }
  }
  return std::make_unique<
      GenericMergeJoin<velocypack::Slice, LocalDocumentId, Comparator>>(
      std::move(desc));
}
}  // namespace

auto IndexJoinStrategyFactory::createStrategy(
    std::vector<Descriptor> desc,
    aql::QueryOptions::JoinStrategyType joinStrategy,
    IndexJoinKeyOrder keyOrder) -> std::unique_ptr<AqlIndexJoinStrategy> {
  switch (keyOrder) {
    case IndexJoinKeyOrder::kBinary:
      return createStrategyWithComparator<
          VPackSliceComparator<IndexJoinKeyOrder::kBinary>>(std::move(desc),
                                                            joinStrategy);
    case IndexJoinKeyOrder::kVPackLegacy:
      return createStrategyWithComparator<
          VPackSliceComparator<IndexJoinKeyOrder::kVPackLegacy>>(
          std::move(desc), joinStrategy);
    case IndexJoinKeyOrder::kVPack:
      break;
  }
  return createStrategyWithComparator<
      VPackSliceComparator<IndexJoinKeyOrder::kVPack>>(std::move(desc),
                                                       joinStrategy);
}

}  // namespace arangodb::aql

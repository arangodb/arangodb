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

#include "Aql/Optimizer/Utils/GetIndexForSortCondition.h"

#include "Indexes/Index.h"
#include "Aql/IndexHint.h"
#include "Aql/SortCondition.h"
#include "Aql/Collection.h"

namespace arangodb::aql::optimizer {
/// @brief Gets the best fitting index for an AQL sort condition
/// note: the caller must have read-locked the underlying collection when
/// calling this method
bool getIndexForSortCondition(Collection const& coll,
                              SortCondition const* sortCondition,
                              Variable const* reference, size_t itemsInIndex,
                              IndexHint const& hint,
                              std::vector<std::shared_ptr<Index>>& usedIndexes,
                              size_t& coveredAttributes) {
  if (!hint.isDisabled()) {
    // We do not have a condition. But we have a sort!
    if (!sortCondition->isEmpty() && sortCondition->isOnlyAttributeAccess()) {
      double bestCost = 0.0;
      std::shared_ptr<Index> bestIndex;

      auto considerIndex =
          [reference, sortCondition, itemsInIndex, &bestCost, &bestIndex,
           &coveredAttributes](std::shared_ptr<Index> const& idx) -> void {
        TRI_ASSERT(!idx->inProgress());

        Index::SortCosts costs =
            idx->supportsSortCondition(sortCondition, reference, itemsInIndex);
        if (costs.supportsCondition &&
            (bestIndex == nullptr || costs.estimatedCosts < bestCost)) {
          bestCost = costs.estimatedCosts;
          bestIndex = idx;
          coveredAttributes = costs.coveredAttributes;
        }
      };

      auto indexes = coll.indexes();

      if (hint.isSimple()) {
        std::vector<std::string> const& hintedIndices = hint.candidateIndexes();
        for (std::string const& hinted : hintedIndices) {
          std::shared_ptr<Index> matched;
          for (std::shared_ptr<Index> const& idx : indexes) {
            if (idx->inProgress()) {
              continue;
            }
            if (idx->name() == hinted) {
              matched = idx;
              break;
            }
          }

          if (matched != nullptr) {
            considerIndex(matched);
            if (bestIndex != nullptr) {
              break;
            }
          }
        }

        if (hint.isForced() && bestIndex == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,
              "could not use index hint to serve query; " + hint.toString());
        }
      }

      if (bestIndex == nullptr) {
        for (auto const& idx : indexes) {
          if (idx->inProgress()) {
            continue;
          }
          if (!Index::onlyHintForced(idx->type())) {
            considerIndex(idx);
          }
        }
      }

      if (bestIndex != nullptr) {
        usedIndexes.emplace_back(bestIndex);
      }

      return bestIndex != nullptr;
    }
  }  // disableIndex

  // No Index and no sort condition that
  // can be supported by an index.
  // Nothing to do here.
  return false;
}
}  // namespace arangodb::aql::optimizer

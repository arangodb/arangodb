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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/DownCast.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/Search.h"

namespace arangodb::iresearch {

// TODO(IResearch): deduplicate these functions with IResearchViewNode

inline IResearchSortBase const& getPrimarySort(
    std::shared_ptr<SearchMeta const> const& meta,
    std::shared_ptr<LogicalView const> const& view) {
  if (meta) {
    TRI_ASSERT(!view || view->type() == ViewType::kSearchAlias);
    return meta->primarySort;
  }
  TRI_ASSERT(view);
  TRI_ASSERT(view->type() == ViewType::kArangoSearch);
  if (ServerState::instance()->isCoordinator()) {
    auto const& viewImpl = basics::downCast<IResearchViewCoordinator>(*view);
    return viewImpl.primarySort();
  }
  auto const& viewImpl = basics::downCast<IResearchView>(*view);
  return viewImpl.primarySort();
}

inline IResearchViewStoredValues const& getStoredValues(
    std::shared_ptr<SearchMeta const> const& meta,
    std::shared_ptr<LogicalView const> const& view) {
  if (meta) {
    TRI_ASSERT(!view || view->type() == ViewType::kSearchAlias);
    return meta->storedValues;
  }
  TRI_ASSERT(view);
  TRI_ASSERT(view->type() == ViewType::kArangoSearch);
  if (ServerState::instance()->isCoordinator()) {
    auto const& viewImpl = basics::downCast<IResearchViewCoordinator>(*view);
    return viewImpl.storedValues();
  }
  auto const& viewImpl = basics::downCast<IResearchView>(*view);
  return viewImpl.storedValues();
}

}  // namespace arangodb::iresearch

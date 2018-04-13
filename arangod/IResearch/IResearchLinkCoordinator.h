////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_LINK_COORDINATOR_H
#define ARANGODB_IRESEARCH__IRESEARCH_LINK_COORDINATOR_H 1

#include "Indexes/Index.h"

namespace arangodb {
namespace iresearch {

std::shared_ptr<Index> createIResearchMMFilesLinkCoordinator(
  arangodb::LogicalCollection* collection,
  arangodb::velocypack::Slice const& definition,
  TRI_idx_iid_t id,
  bool isClusterConstructor
) noexcept;

std::shared_ptr<Index> createIResearchRocksDBLinkCoordinator(
  arangodb::LogicalCollection* collection,
  arangodb::velocypack::Slice const& definition,
  TRI_idx_iid_t id,
  bool isClusterConstructor
) noexcept;

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VIEW_LINK_COORDINATOR_H 


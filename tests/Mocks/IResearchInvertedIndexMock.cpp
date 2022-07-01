////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

#include "IResearchInvertedIndexMock.h"
#include "IResearch/IResearchLinkHelper.h"

namespace arangodb {
namespace iresearch {

IResearchInvertedIndexMock::IResearchInvertedIndexMock(
    IndexId iid, arangodb::LogicalCollection &collection)
    : Index(iid, collection, IResearchLinkHelper::emptyIndexSlice(0).slice()),
      IResearchInvertedIndex(iid, collection) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  _unique = false; // cannot be unique since multiple fields are indexed
  _sparse = true;  // always sparse
}

} // namespace iresearch
} // namespace arangodb

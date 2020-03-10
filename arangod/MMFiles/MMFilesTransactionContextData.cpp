////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesTransactionContextData.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesDitch.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

MMFilesTransactionContextData::MMFilesTransactionContextData()
    : _lastPinnedCid(0) {}

MMFilesTransactionContextData::~MMFilesTransactionContextData() {
  for (auto& it : _ditches) {
    // we're done with this ditch
    auto& ditch = it.second;
    ditch->ditches()->freeMMFilesDocumentDitch(ditch, true /* fromTransaction */);
    // If some external entity is still using the ditch, it is kept!
  }
}

/// @brief pin data for the collection
void MMFilesTransactionContextData::pinData(LogicalCollection* collection) {
  TRI_voc_cid_t const cid = collection->id();

  if (_lastPinnedCid == cid) {
    // already pinned data for this collection
    return;
  }

  auto it = _ditches.find(cid);

  if (it != _ditches.end()) {
    // tell everyone else this ditch is still in use,
    // at least until the transaction is over
    TRI_ASSERT((*it).second->usedByTransaction());
    // ditch already exists
    return;
  }

  // this method will not throw, but may return a nullptr
  auto ditch = arangodb::MMFilesCollection::toMMFilesCollection(collection)
                   ->ditches()
                   ->createMMFilesDocumentDitch(true, __FILE__, __LINE__);

  if (ditch == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  try {
    _ditches.try_emplace(cid, ditch);
  } catch (...) {
    ditch->ditches()->freeMMFilesDocumentDitch(ditch, true);
    throw;
  }

  _lastPinnedCid = cid;
}

/// @brief whether or not the data for the collection is pinned
bool MMFilesTransactionContextData::isPinned(TRI_voc_cid_t cid) const {
  return (_ditches.find(cid) != _ditches.end());
}

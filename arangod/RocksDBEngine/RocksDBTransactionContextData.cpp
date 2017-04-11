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

#include "RocksDBTransactionContextData.h"
#include "RocksDBCollection.h"
#include "VocBase/LogicalCollection.h"
#include <stdexcept>

using namespace arangodb;

RocksDBTransactionContextData::RocksDBTransactionContextData(){
}

RocksDBTransactionContextData::~RocksDBTransactionContextData() {
}

/// @brief pin data for the collection
void RocksDBTransactionContextData::pinData(LogicalCollection* collection) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
}

/// @brief whether or not the data for the collection is pinned
bool RocksDBTransactionContextData::isPinned(TRI_voc_cid_t cid) const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

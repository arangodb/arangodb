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

#include "RocksDBKeyComparator.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "Indexes/RocksDBIndex.h"
#include "Logger/Logger.h"

#include <rocksdb/db.h>
#include <rocksdb/comparator.h>

using namespace arangodb;

int RocksDBKeyComparator::Compare(rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const {
  TRI_ASSERT(lhs.size() > 8);
  TRI_ASSERT(rhs.size() > 8);

  // compare by index id first
  int res = memcmp(lhs.data(), rhs.data(), RocksDBIndex::keyPrefixSize());

  if (res != 0) {
    return res;
  }
  
  VPackSlice const lSlice = extractKeySlice(lhs);
  TRI_ASSERT(lSlice.isArray());
  VPackSlice const rSlice = extractKeySlice(rhs);
  TRI_ASSERT(rSlice.isArray());

  size_t const lLength = lSlice.length();
  size_t const rLength = rSlice.length();
  size_t const n = lLength < rLength ? lLength : rLength;

  for (size_t i = 0; i < n; ++i) {
    int res = arangodb::basics::VelocyPackHelper::compare(
      (i < lLength ? lSlice[i] : VPackSlice::noneSlice()), 
      (i < rLength ? rSlice[i] : VPackSlice::noneSlice()), 
      true
    );

    if (res != 0) {
      return res;
    }
  }

  if (lLength != rLength) {
    return lLength < rLength ? -1 : 1;
  }

  return 0;
}


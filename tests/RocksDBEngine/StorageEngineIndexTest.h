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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/StorageEngineDocumentTest.h"

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

#include <velocypack/Slice.h>

#include <memory>
#include <tuple>
#include <vector>

namespace arangodb::tests {

// Adds secondary-index creation/inspection on top of the document-level
// fixture
class StorageEngineIndexTest : public StorageEngineDocumentTest {
 protected:
  // waitAndGet() is the
  // safe way to drive createIndex() to completion synchronously
  std::shared_ptr<Index> makeIndex(VPackSlice definition) {
    bool created = false;
    auto index = _collection->createIndex(definition, created).waitAndGet();
    EXPECT_TRUE(created);
    return index;
  }

  std::vector<LocalDocumentId> scanIndex(Index& index) {
    SingleCollectionTransaction trx{context(), *_collection,
                                    AccessMode::Type::READ};
    EXPECT_TRUE(IsOk(trx.begin()));
    ResourceMonitor monitor{GlobalResourceMonitor::instance()};
    IndexIteratorOptions opts;
    opts.sorted = index.isSorted();
    auto iterator = index.iteratorForCondition(monitor, &trx, nullptr, nullptr,
                                               opts, ReadOwnWrites::no, -1);
    std::vector<LocalDocumentId> ids;
    iterator->all([&](LocalDocumentId id) {
      ids.push_back(id);
      return true;
    });
    std::ignore = trx.finish(Result{});
    return ids;
  }
};

}  // namespace arangodb::tests

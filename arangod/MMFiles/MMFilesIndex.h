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

#ifndef ARANGOD_MMFILES_MMFILES_INDEX_H
#define ARANGOD_MMFILES_MMFILES_INDEX_H 1

#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Indexes/Index.h"

namespace arangodb {
class LogicalCollection;

namespace velocypack {
class Slice;
}

class MMFilesIndex : public Index {
 public:
  MMFilesIndex(TRI_idx_iid_t id, LogicalCollection& collection, std::string const& name,
               std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
               bool unique, bool sparse)
      : Index(id, collection, name, attributes, unique, sparse) {}

  MMFilesIndex(TRI_idx_iid_t id, LogicalCollection& collection,
               arangodb::velocypack::Slice const& info)
      : Index(id, collection, info) {}

  /// @brief if true this index should not be shown externally
  virtual bool isHidden() const override {
    return false;  // do not generally hide MMFiles indexes
  }

  virtual Result sizeHint(transaction::Methods& trx, size_t size) { return Result(); }

  virtual bool isPersistent() const override { return false; };

  virtual void batchInsert(transaction::Methods& trx,
                           std::vector<std::pair<LocalDocumentId, arangodb::velocypack::Slice>> const& docs,
                           std::shared_ptr<arangodb::basics::LocalTaskQueue> queue);

  virtual Result insert(transaction::Methods& trx, LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const& doc, OperationMode mode) = 0;

  virtual Result remove(transaction::Methods& trx, LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const& doc, OperationMode mode) = 0;

  void afterTruncate(TRI_voc_tick_t) override {
    // for mmfiles, truncating the index just unloads it
    unload();
  }
};
}  // namespace arangodb

#endif

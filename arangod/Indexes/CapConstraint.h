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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_INDEXES_CAP_CONSTRAINT_H
#define ARANGOD_INDEXES_CAP_CONSTRAINT_H 1

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class CapConstraint final : public Index {
  
 public:
  CapConstraint() = delete;

  CapConstraint(TRI_idx_iid_t, struct TRI_document_collection_t*, size_t,
                int64_t);

  ~CapConstraint();

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum number of documents in the collection
  //////////////////////////////////////////////////////////////////////////////

  int64_t count() const { return _count; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum size of documents in the collection
  //////////////////////////////////////////////////////////////////////////////

  int64_t size() const { return _size; }

  IndexType type() const override final {
    return Index::TRI_IDX_TYPE_CAP_CONSTRAINT;
  }

  bool isSorted() const override final { return false; }

  bool hasSelectivityEstimate() const override final { return false; }

  bool dumpFields() const override final { return false; }

  size_t memory() const override final;

  arangodb::basics::Json toJson(TRI_memory_zone_t*, bool) const override final;
  arangodb::basics::Json toJsonFigures(TRI_memory_zone_t*) const override final;

  int insert(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int postInsert(arangodb::Transaction*,
                 struct TRI_transaction_collection_s*,
                 struct TRI_doc_mptr_t const*) override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the cap constraint
  //////////////////////////////////////////////////////////////////////////////

  int initialize(arangodb::Transaction*);

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief apply the cap constraint for the collection
  //////////////////////////////////////////////////////////////////////////////

  int apply(arangodb::Transaction*, TRI_document_collection_t*,
            struct TRI_transaction_collection_s*);

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum number of documents in the collection
  //////////////////////////////////////////////////////////////////////////////

  int64_t const _count;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum size of documents in the collection
  //////////////////////////////////////////////////////////////////////////////

  int64_t const _size;

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief minimum size
  //////////////////////////////////////////////////////////////////////////////

  static int64_t const MinSize;
};
}

#endif



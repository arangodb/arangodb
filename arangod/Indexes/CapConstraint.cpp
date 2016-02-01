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

#include "CapConstraint.h"
#include "Basics/Logger.h"
#include "Utils/transactions.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum size
////////////////////////////////////////////////////////////////////////////////

int64_t const CapConstraint::MinSize = 16384;

CapConstraint::CapConstraint(TRI_idx_iid_t iid,
                             TRI_document_collection_t* collection,
                             size_t count, int64_t size)
    : Index(iid, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(), false,
            false),
      _count(count),
      _size(static_cast<int64_t>(size)) {}

CapConstraint::~CapConstraint() {}

size_t CapConstraint::memory() const { return 0; }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the index
////////////////////////////////////////////////////////////////////////////////

void CapConstraint::toVelocyPack(VPackBuilder& builder,
                                 bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);
  builder.add("size", VPackValue(_count));
  builder.add("byteSize", VPackValue(_size));
  builder.add("unique", VPackValue(false));
}

int CapConstraint::insert(arangodb::Transaction*, TRI_doc_mptr_t const* doc,
                          bool) {
  if (_size > 0) {
    // there is a size restriction
    auto marker = static_cast<TRI_df_marker_t const*>(
        doc->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

    // check if the document would be too big
    if (static_cast<int64_t>(marker->_size) > _size) {
      return TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

int CapConstraint::remove(arangodb::Transaction*, TRI_doc_mptr_t const*, bool) {
  return TRI_ERROR_NO_ERROR;
}

int CapConstraint::postInsert(arangodb::Transaction* trx,
                              TRI_transaction_collection_t* trxCollection,
                              TRI_doc_mptr_t const*) {
  TRI_ASSERT(_count > 0 || _size > 0);

  return apply(trx, trxCollection->_collection->_collection, trxCollection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the cap constraint
////////////////////////////////////////////////////////////////////////////////

int CapConstraint::initialize(arangodb::Transaction* trx) {
  TRI_ASSERT(_count > 0 || _size > 0);

  TRI_headers_t* headers = _collection->_headersPtr;  // ONLY IN INDEX (CAP)
  int64_t currentCount = static_cast<int64_t>(headers->count());
  int64_t currentSize = headers->size();

  if ((_count > 0 && currentCount <= _count) &&
      (_size > 0 && currentSize <= _size)) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  } else {
    TRI_vocbase_t* vocbase = _collection->_vocbase;
    TRI_voc_cid_t cid = _collection->_info.id();

    arangodb::SingleCollectionWriteTransaction<UINT64_MAX> trx(
        new arangodb::StandaloneTransactionContext(), vocbase, cid);
    trx.addHint(TRI_TRANSACTION_HINT_LOCK_NEVER, false);
    trx.addHint(TRI_TRANSACTION_HINT_NO_BEGIN_MARKER, false);
    trx.addHint(TRI_TRANSACTION_HINT_NO_ABORT_MARKER, false);
    trx.addHint(
        TRI_TRANSACTION_HINT_SINGLE_OPERATION,
        false);  // this is actually not true, but necessary to create trx id 0

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_transaction_collection_t* trxCollection = trx.trxCollection();
    res = apply(&trx, _collection, trxCollection);

    res = trx.finish(res);

    return res;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the cap constraint for the collection
////////////////////////////////////////////////////////////////////////////////

int CapConstraint::apply(arangodb::Transaction* trx,
                         TRI_document_collection_t* document,
                         TRI_transaction_collection_t* trxCollection) {
  TRI_headers_t* headers =
      document->_headersPtr;  // PROTECTED by trx in trxCollection
  int64_t currentCount = static_cast<int64_t>(headers->count());
  int64_t currentSize = headers->size();

  int res = TRI_ERROR_NO_ERROR;

  // delete while at least one of the constraints is still violated
  while ((_count > 0 && currentCount > _count) ||
         (_size > 0 && currentSize > _size)) {
    TRI_doc_mptr_t* oldest = headers->front();

    if (oldest != nullptr) {
      TRI_ASSERT(oldest->getDataPtr() !=
                 nullptr);  // ONLY IN INDEX, PROTECTED by RUNTIME
      size_t oldSize = ((TRI_df_marker_t*)(oldest->getDataPtr()))
                           ->_size;  // ONLY IN INDEX, PROTECTED by RUNTIME

      TRI_ASSERT(oldSize > 0);

      if (trxCollection != nullptr) {
        res = TRI_DeleteDocumentDocumentCollection(trx, trxCollection, nullptr,
                                                   oldest);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG(WARN) << "cannot cap collection: " << TRI_errno_string(res);
          break;
        }
      } else {
        headers->unlink(oldest);
      }

      currentCount--;
      currentSize -= (int64_t)oldSize;
    } else {
      // we should not get here
      LOG(WARN) << "logic error in " << __FUNCTION__;
      break;
    }
  }

  return res;
}

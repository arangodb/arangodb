////////////////////////////////////////////////////////////////////////////////
/// @brief cap constraint
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "CapConstraint.h"
#include "Utils/transactions.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                               class CapConstraint
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum size
////////////////////////////////////////////////////////////////////////////////

int64_t const CapConstraint::MinSize = 16384;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

CapConstraint::CapConstraint (TRI_idx_iid_t iid,
                              TRI_document_collection_t* collection,
                              size_t count,
                              int64_t size)
  : Index(iid, collection, std::vector<std::vector<triagens::basics::AttributeName>>(), false, false),
    _count(count),
    _size(static_cast<int64_t>(size)) {

  initialize();
}

CapConstraint::~CapConstraint () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
        
size_t CapConstraint::memory () const {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index
////////////////////////////////////////////////////////////////////////////////

triagens::basics::Json CapConstraint::toJson (TRI_memory_zone_t* zone, 
                                              bool withFigures) const {
  auto json = Index::toJson(zone, withFigures);

  json("size",     triagens::basics::Json(zone, static_cast<double>(_count)))
      ("byteSize", triagens::basics::Json(zone, static_cast<double>(_size)))
      ("unique",   triagens::basics::Json(zone, false));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index figures
////////////////////////////////////////////////////////////////////////////////
        
triagens::basics::Json CapConstraint::toJsonFigures (TRI_memory_zone_t* zone) const {
  triagens::basics::Json json(triagens::basics::Json::Object);
  json("memory", triagens::basics::Json(static_cast<double>(memory())));

  return json;
}
  
int CapConstraint::insert (TRI_doc_mptr_t const* doc, 
                           bool) {
  if (_size > 0) {
    // there is a size restriction
    auto marker = static_cast<TRI_df_marker_t const*>(doc->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

    // check if the document would be too big
    if (static_cast<int64_t>(marker->_size) > _size) {
      return TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE;
    }
  }

  return TRI_ERROR_NO_ERROR;
}
         
int CapConstraint::remove (TRI_doc_mptr_t const*, 
                           bool) {
  return TRI_ERROR_NO_ERROR;
}
        
int CapConstraint::postInsert (TRI_transaction_collection_t* trxCollection, 
                               TRI_doc_mptr_t const*) {
  TRI_ASSERT(_count > 0 || _size > 0);

  return apply(trxCollection->_collection->_collection, trxCollection);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the cap constraint
////////////////////////////////////////////////////////////////////////////////

int CapConstraint::initialize () {
  TRI_ASSERT(_count > 0 || _size > 0);

  TRI_headers_t* headers = _collection->_headersPtr;  // ONLY IN INDEX (CAP)
  int64_t currentCount   = static_cast<int64_t>(headers->count());
  int64_t currentSize    = headers->size();

  if ((_count > 0 && currentCount <= _count) &&
      (_size > 0 && currentSize <= _size)) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }
  else {
    TRI_vocbase_t* vocbase = _collection->_vocbase;
    TRI_voc_cid_t cid = _collection->_info._cid;

    triagens::arango::SingleCollectionWriteTransaction<UINT64_MAX> trx(new triagens::arango::StandaloneTransactionContext(), vocbase, cid);
    trx.addHint(TRI_TRANSACTION_HINT_LOCK_NEVER, false);
    trx.addHint(TRI_TRANSACTION_HINT_NO_BEGIN_MARKER, false);
    trx.addHint(TRI_TRANSACTION_HINT_NO_ABORT_MARKER, false);
    trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false); // this is actually not true, but necessary to create trx id 0

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_transaction_collection_t* trxCollection = trx.trxCollection();
    res = apply(_collection, trxCollection);

    res = trx.finish(res);

    return res;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the cap constraint for the collection
////////////////////////////////////////////////////////////////////////////////

int CapConstraint::apply (TRI_document_collection_t* document,
                          TRI_transaction_collection_t* trxCollection) {
  TRI_headers_t* headers = document->_headersPtr;  // PROTECTED by trx in trxCollection
  int64_t currentCount   = static_cast<int64_t>(headers->count());
  int64_t currentSize    = headers->size();

  int res = TRI_ERROR_NO_ERROR;

  // delete while at least one of the constraints is still violated
  while ((_count > 0 && currentCount > _count) ||
         (_size > 0 && currentSize > _size)) {
    TRI_doc_mptr_t* oldest = headers->front();

    if (oldest != nullptr) {
      TRI_ASSERT(oldest->getDataPtr() != nullptr);  // ONLY IN INDEX, PROTECTED by RUNTIME
      size_t oldSize = ((TRI_df_marker_t*) (oldest->getDataPtr()))->_size;  // ONLY IN INDEX, PROTECTED by RUNTIME

      TRI_ASSERT(oldSize > 0);

      if (trxCollection != nullptr) {
        res = TRI_DeleteDocumentDocumentCollection(trxCollection, nullptr, oldest);

        if (res != TRI_ERROR_NO_ERROR) {
          LOG_WARNING("cannot cap collection: %s", TRI_errno_string(res));
          break;
        }
      }
      else {
        headers->unlink(oldest);
      }

      currentCount--;
      currentSize -= (int64_t) oldSize;
    }
    else {
      // we should not get here
      LOG_WARNING("logic error in %s", __FUNCTION__);
      break;
    }
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

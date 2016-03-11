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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Transaction.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Indexes/PrimaryIndex.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationCursor.h"
#include "Utils/TransactionContext.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/Ditch.h"
#include "VocBase/document-collection.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/server.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief if this pointer is set to an actual set, then for each request
/// sent to a shardId using the ClusterComm library, an X-Arango-Nolock
/// header is generated.
////////////////////////////////////////////////////////////////////////////////

thread_local std::unordered_set<std::string>* Transaction::_makeNolockHeaders =
    nullptr;
  
////////////////////////////////////////////////////////////////////////////////
/// @brief Index Iterator Context
////////////////////////////////////////////////////////////////////////////////

struct OpenIndexIteratorContext {
  arangodb::Transaction* trx;
  TRI_document_collection_t* collection;
};
      
Transaction::Transaction(std::shared_ptr<TransactionContext> transactionContext,
                         TRI_voc_tid_t externalId)
    : _externalId(externalId),
      _setupState(TRI_ERROR_NO_ERROR),
      _nestingLevel(0),
      _errorData(),
      _hints(0),
      _timeout(0.0),
      _waitForSync(false),
      _allowImplicitCollections(true),
      _isReal(true),
      _trx(nullptr),
      _vocbase(transactionContext->vocbase()),
      _transactionContext(transactionContext) {
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_transactionContext != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    _isReal = false;
  }

  this->setupTransaction();
}
   
////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction
////////////////////////////////////////////////////////////////////////////////

Transaction::~Transaction() {
  if (_trx == nullptr) {
    return;
  }

  if (isEmbeddedTransaction()) {
    _trx->_nestingLevel--;
  } else {
    if (getStatus() == TRI_TRANSACTION_RUNNING) {
      // auto abort a running transaction
      this->abort();
    }

    // free the data associated with the transaction
    freeTransaction();
  }
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief return the names of all collections used in the transaction
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Transaction::collectionNames() const {
  std::vector<std::string> result;

  for (size_t i = 0; i < _trx->_collections._length; ++i) {
    auto trxCollection = static_cast<TRI_transaction_collection_t*>(
        TRI_AtVectorPointer(&_trx->_collections, i));

    if (trxCollection->_collection != nullptr) {
      result.emplace_back(trxCollection->_collection->_name);
    }
  }

  return result;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection name resolver
////////////////////////////////////////////////////////////////////////////////

CollectionNameResolver const* Transaction::resolver() const {
  CollectionNameResolver const* r = this->_transactionContext->getResolver();
  TRI_ASSERT(r != nullptr);
  return r;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief return the transaction collection for a document collection
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_collection_t* Transaction::trxCollection(TRI_voc_cid_t cid) const {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  return TRI_GetCollectionTransaction(_trx, cid, TRI_TRANSACTION_READ);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief order a ditch for a collection
////////////////////////////////////////////////////////////////////////////////

DocumentDitch* Transaction::orderDitch(TRI_voc_cid_t cid) {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING ||
             getStatus() == TRI_TRANSACTION_CREATED);

  TRI_transaction_collection_t* trxCollection = TRI_GetCollectionTransaction(_trx, cid, TRI_TRANSACTION_READ);

  if (trxCollection == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);    
  }

  TRI_ASSERT(trxCollection->_collection != nullptr);

  TRI_document_collection_t* document =
      trxCollection->_collection->_collection;
  TRI_ASSERT(document != nullptr);

  DocumentDitch* ditch = _transactionContext->orderDitch(document);

  if (ditch == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return ditch;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _key attribute from a slice
////////////////////////////////////////////////////////////////////////////////

std::string Transaction::extractKey(VPackSlice const slice) {
  // extract _key
  if (slice.isObject()) {
    VPackSlice k = slice.get(TRI_VOC_ATTRIBUTE_KEY);
    if (!k.isString()) {
      return ""; // fail
    }
    return k.copyString();
  } 
  if (slice.isString()) {
    return slice.copyString();
  } 
  return "";
}

//////////////////////////////////////////////////////////////////////////////
/// @brief extract the _id attribute from a slice, and convert it into a 
/// string
//////////////////////////////////////////////////////////////////////////////

std::string Transaction::extractIdString(VPackSlice const slice) {
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
      
  VPackSlice const id = slice.get(TRI_VOC_ATTRIBUTE_ID);
  if (id.isString()) {
    // _id is already a string
    return id.copyString();
  }

  if (!id.isCustom() || id.head() != 0xf3) {
    // invalid type for _id
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  VPackSlice const key = slice.get(TRI_VOC_ATTRIBUTE_KEY);
  if (!key.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  
  uint64_t cid = DatafileHelper::ReadNumber<uint64_t>(id.begin() + 1, sizeof(uint64_t));
  char buffer[512];  // This is enough for collection name + _key
  size_t len = resolver()->getCollectionName(&buffer[0], cid);
  buffer[len] = '/';

  VPackValueLength keyLength;
  char const* p = key.getString(keyLength);
  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid _key value");
  }
  memcpy(&buffer[len + 1], p, keyLength);
  return std::string(&buffer[0], len + 1 + keyLength);
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief build a VPack object with _id, _key and _rev, the result is
/// added to the builder in the argument as a single object.
//////////////////////////////////////////////////////////////////////////////

void Transaction::buildDocumentIdentity(VPackBuilder& builder,
                                        TRI_voc_cid_t cid,
                                        std::string const& key,
                                        VPackSlice const rid,
                                        VPackSlice const oldRid,
                                        TRI_doc_mptr_t const* oldMptr,
                                        TRI_doc_mptr_t const* newMptr) {
  std::string collectionName = resolver()->getCollectionName(cid);

  builder.openObject();
  builder.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(collectionName + "/" + key));
  builder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
  TRI_ASSERT(!rid.isNone());
  builder.add(TRI_VOC_ATTRIBUTE_REV, rid);
  if (!oldRid.isNone()) {
    builder.add("_oldRev", oldRid);
  }
  if (oldMptr != nullptr) {
    builder.add("old", VPackSlice(oldMptr->vpack()));
#warning Add externals later.
    //builder.add("old", VPackValue(VPackValueType::External, oldMptr->vpack()));
  }
  if (newMptr != nullptr) {
    builder.add("new", VPackSlice(newMptr->vpack()));
#warning Add externals later.
    //builder.add("new", VPackValue(VPackValueType::External, newMptr->vpack()));
  }
  builder.close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin the transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::begin() {
  if (_trx == nullptr) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  if (_setupState != TRI_ERROR_NO_ERROR) {
    return _setupState;
  }

  if (!_isReal) {
    if (_nestingLevel == 0) {
      _trx->_status = TRI_TRANSACTION_RUNNING;
    }
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_BeginTransaction(_trx, _hints, _nestingLevel);

  return res;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief commit / finish the transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::commit() {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    // transaction not created or not running
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  if (!_isReal) {
    if (_nestingLevel == 0) {
      _trx->_status = TRI_TRANSACTION_COMMITTED;
    }
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_CommitTransaction(_trx, _nestingLevel);

  return res;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief abort the transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::abort() {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    // transaction not created or not running
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  if (!_isReal) {
    if (_nestingLevel == 0) {
      _trx->_status = TRI_TRANSACTION_ABORTED;
    }

    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_AbortTransaction(_trx, _nestingLevel);

  return res;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief finish a transaction (commit or abort), based on the previous state
////////////////////////////////////////////////////////////////////////////////

int Transaction::finish(int errorNum) {
  if (errorNum == TRI_ERROR_NO_ERROR) {
    // there was no previous error, so we'll commit
    return this->commit();
  }
  
  // there was a previous error, so we'll abort
  this->abort();

  // return original error number
  return errorNum;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read any (random) document
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::any(std::string const& collectionName) {
  return any(collectionName, 0, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers, using skip and limit.
/// The resualt guarantees that all documents are contained exactly once
/// as long as the collection is not modified.
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::any(std::string const& collectionName,
                                 uint64_t skip, uint64_t limit) {
  if (ServerState::instance()->isCoordinator()) {
    return anyCoordinator(collectionName, skip, limit);
  }
  return anyLocal(collectionName, skip, limit);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches documents in a collection in random order, coordinator
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::anyCoordinator(std::string const&, uint64_t,
                                            uint64_t) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches documents in a collection in random order, local
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::anyLocal(std::string const& collectionName,
                                      uint64_t skip, uint64_t limit) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
 
  orderDitch(cid); // will throw when it fails 
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
  
  VPackBuilder resultBuilder;
  resultBuilder.openArray();
  
  OperationCursor cursor = indexScan(collectionName, Transaction::CursorType::ANY, "", {}, skip, limit, 1000, false);

  while (cursor.hasMore()) {
    int res = cursor.getMore();

    if (res != TRI_ERROR_NO_ERROR) {
      return OperationResult(res);
    }
  
    VPackSlice docs = cursor.slice();
    VPackArrayIterator it(docs);
    while (it.valid()) {
      resultBuilder.add(it.value());
      it.next();
    }
  }

  resultBuilder.close();

  res = unlock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationCursor(res);
  }

  return OperationResult(resultBuilder.steal(),
                         transactionContext()->orderCustomTypeHandler(), "",
                         TRI_ERROR_NO_ERROR, false);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the type of a collection
//////////////////////////////////////////////////////////////////////////////

bool Transaction::isEdgeCollection(std::string const& collectionName) {
  return getCollectionType(collectionName) == TRI_COL_TYPE_EDGE;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the type of a collection
//////////////////////////////////////////////////////////////////////////////

bool Transaction::isDocumentCollection(std::string const& collectionName) {
  return getCollectionType(collectionName) == TRI_COL_TYPE_DOCUMENT;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the type of a collection
//////////////////////////////////////////////////////////////////////////////
  
TRI_col_type_t Transaction::getCollectionType(std::string const& collectionName) {
  if (ServerState::instance()->isCoordinator()) {
    return resolver()->getCollectionTypeCluster(collectionName);
  }
  return resolver()->getCollectionType(collectionName);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the name of a collection
//////////////////////////////////////////////////////////////////////////////
  
std::string Transaction::collectionName(TRI_voc_cid_t cid) { 
  return resolver()->getCollectionName(cid);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Iterate over all elements of the collection.
//////////////////////////////////////////////////////////////////////////////

void Transaction::invokeOnAllElements(std::string const& collectionName,
                                      std::function<bool(TRI_doc_mptr_t const*)> callback) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  if (ServerState::instance()->isCoordinator()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  TRI_transaction_collection_t* trxCol = trxCollection(cid);

  TRI_document_collection_t* document = documentCollection(trxCol);

  orderDitch(cid); // will throw when it fails

  int res = lock(trxCol, TRI_TRANSACTION_READ);
  
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto primaryIndex = document->primaryIndex();
  primaryIndex->invokeOnAllElements(callback);
  
  res = unlock(trxCol, TRI_TRANSACTION_READ);
  
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return one or multiple documents from a collection
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::document(std::string const& collectionName,
                                      VPackSlice const value,
                                      OperationOptions& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (ServerState::instance()->isCoordinator()) {
    return documentCoordinator(collectionName, value, options);
  }

  return documentLocal(collectionName, value, options);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief read one or multiple documents in a collection, coordinator
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::documentCoordinator(std::string const& collectionName,
                                                 VPackSlice const value,
                                                 OperationOptions& options) {
  if (value.isArray()) {
    // multi-document variant is not yet implemented
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  auto headers = std::make_unique<std::map<std::string, std::string>>();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  std::string key(Transaction::extractKey(value));
  if (key.empty()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }
  TRI_voc_rid_t expectedRevision = TRI_ExtractRevisionId(value);

  int res = arangodb::getDocumentOnCoordinator(
      _vocbase->_name, collectionName, key, expectedRevision, headers, true,
      responseCode, resultHeaders, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    if (responseCode == arangodb::rest::HttpResponse::OK ||
        responseCode == arangodb::rest::HttpResponse::PRECONDITION_FAILED) {
      VPackParser parser;
      try {
        parser.parse(resultBody);
        auto bui = parser.steal();
        auto buf = bui->steal();
        return OperationResult(buf, nullptr, "", 
            responseCode == arangodb::rest::HttpResponse::OK ?
            TRI_ERROR_NO_ERROR : TRI_ERROR_ARANGO_CONFLICT, 
            TRI_ERROR_NO_ERROR);
      }
      catch (VPackException& e) {
        std::string message = "JSON from DBserver not parseable: "
                              + resultBody + ":" + e.what();
        return OperationResult(TRI_ERROR_INTERNAL, message);
      }
    } else if (responseCode == arangodb::rest::HttpResponse::NOT_FOUND) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    } else {
      return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief read one or multiple documents in a collection, local
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::documentLocal(std::string const& collectionName,
                                           VPackSlice const value,
                                           OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));

  orderDitch(cid); // will throw when it fails
 
  VPackBuilder resultBuilder;

  auto workOnOneDocument = [&](VPackSlice const value) -> int {
    std::string key(Transaction::extractKey(value));
    if (key.empty()) {
      return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
    }

    VPackSlice expectedRevision;
    if (!options.ignoreRevs) {
      expectedRevision = TRI_ExtractRevisionIdAsSlice(value);
    }

    TRI_doc_mptr_t mptr;
    int res = document->read(this, key, &mptr, !isLocked(document, TRI_TRANSACTION_READ));

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  
    TRI_ASSERT(mptr.getDataPtr() != nullptr);
    if (!expectedRevision.isNone()) {
      VPackSlice foundRevision = mptr.revisionIdAsSlice();
      if (expectedRevision != foundRevision) {
        // still return 
        buildDocumentIdentity(resultBuilder, cid, key, foundRevision, 
                              VPackSlice(), nullptr, nullptr);
        return TRI_ERROR_ARANGO_CONFLICT;
      }
    }
  
    if (!options.silent) {
      //resultBuilder.add(VPackValue(static_cast<void const*>(mptr.vpack()), VPackValueType::External));
      // This is the future, for now, we have to do this:
      resultBuilder.add(VPackSlice(mptr.vpack()));
    }

    return TRI_ERROR_NO_ERROR;
  };

  int res = TRI_ERROR_NO_ERROR;
  if (!value.isArray()) {
    res = workOnOneDocument(value);
  } else {
    VPackArrayBuilder guard(&resultBuilder);
    for (auto const s : VPackArrayIterator(value)) {
      res = workOnOneDocument(s);
      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
  }

  return OperationResult(resultBuilder.steal(), 
                         transactionContext()->orderCustomTypeHandler(), "",
                         res, options.waitForSync); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief create one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::insert(std::string const& collectionName,
                                    VPackSlice const value,
                                    OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // Validate Edges
  if (isEdgeCollection(collectionName)) {
    // Check _from
    auto checkFrom = [&](VPackSlice const value) -> void {
      size_t split;
      VPackSlice from = value.get(TRI_VOC_ATTRIBUTE_FROM);
      if (!from.isString()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
      }
      std::string docId = from.copyString();
      if (!TRI_ValidateDocumentIdKeyGenerator(docId.c_str(), &split)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
      }
    };

    // Check _to
    auto checkTo = [&](VPackSlice const value) -> void {
      size_t split;
      VPackSlice to = value.get(TRI_VOC_ATTRIBUTE_TO);
      if (!to.isString()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
      }
      std::string docId = to.copyString();
      if (!TRI_ValidateDocumentIdKeyGenerator(docId.c_str(), &split)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE);
      }
    };

    if (value.isArray()) {
      for (auto s : VPackArrayIterator(value)) {
        checkFrom(s);
        checkTo(s);
      }
    } else {
      checkFrom(value);
      checkTo(value);
    }
  }

  OperationOptions optionsCopy = options;

  if (ServerState::instance()->isCoordinator()) {
    return insertCoordinator(collectionName, value, optionsCopy);
  }

  return insertLocal(collectionName, value, optionsCopy);
}
   
//////////////////////////////////////////////////////////////////////////////
/// @brief create one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::insertCoordinator(std::string const& collectionName,
                                               VPackSlice const value,
                                               OperationOptions& options) {

  if (value.isArray()) {
    // must provide a document object
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  std::map<std::string, std::string> headers;
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  int res = arangodb::createDocumentOnCoordinator(
      _vocbase->_name, collectionName, options.waitForSync,
      value, headers, responseCode, resultHeaders, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    if (responseCode == arangodb::rest::HttpResponse::ACCEPTED ||
        responseCode == arangodb::rest::HttpResponse::CREATED) {
      VPackParser parser;
      try {
        parser.parse(resultBody);
        auto bui = parser.steal();
        auto buf = bui->steal();
        return OperationResult(buf, nullptr, "", TRI_ERROR_NO_ERROR, 
            responseCode == arangodb::rest::HttpResponse::CREATED);
      }
      catch (VPackException& e) {
        std::string message = "JSON from DBserver not parseable: "
                              + resultBody + ":" + e.what();
        return OperationResult(TRI_ERROR_INTERNAL, message);
      }
    } else if (responseCode == arangodb::rest::HttpResponse::PRECONDITION_FAILED) {
      return OperationResult(TRI_ERROR_ARANGO_CONFLICT);
    } else if (responseCode == arangodb::rest::HttpResponse::BAD) {
      return OperationResult(TRI_ERROR_INTERNAL,
                             "JSON sent to DBserver was bad");
    } else if (responseCode == arangodb::rest::HttpResponse::NOT_FOUND) {
      return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    } else {
      return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief create one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::insertLocal(std::string const& collectionName,
                                         VPackSlice const value,
                                         OperationOptions& options) {
 
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  TRI_document_collection_t* document = documentCollection(trxCollection(cid));

  VPackBuilder resultBuilder;

  auto workForOneDocument = [&](VPackSlice const value) -> int {
    if (!value.isObject()) {
      return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
    }
    // add missing attributes for document (_id, _rev, _key)
    VPackBuilder merge;
    merge.openObject();
     
    // generate a new tick value
    TRI_voc_tick_t const revisionId = TRI_NewTickServer();
    std::string keyString;
    auto key = value.get(TRI_VOC_ATTRIBUTE_KEY);

    if (key.isNone()) {
      // "_key" attribute not present in object
      keyString = document->_keyGenerator->generate(revisionId);
      merge.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(keyString));
    } else if (!key.isString()) {
      // "_key" present but wrong type
      return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
    } else {
      keyString = key.copyString();
      int res = document->_keyGenerator->validate(keyString, false);

      if (res != TRI_ERROR_NO_ERROR) {
        // invalid key value
        return res;
      }
    }
    
    // add _rev attribute
    merge.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(std::to_string(revisionId)));

    // add _id attribute
    uint8_t* p = merge.add(TRI_VOC_ATTRIBUTE_ID, VPackValuePair(9ULL, VPackValueType::Custom));
    *p++ = 0xf3; // custom type for _id
    DatafileHelper::StoreNumber<uint64_t>(p, cid, sizeof(uint64_t));

    merge.close();

    VPackBuilder toInsert = VPackCollection::merge(value, merge.slice(), false, false); 
    VPackSlice insertSlice = toInsert.slice();
    
    TRI_doc_mptr_t mptr;
    int res = document->insert(this, insertSlice, &mptr, options,
        !isLocked(document, TRI_TRANSACTION_WRITE));
    
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (options.silent) {
      // no need to construct the result object
      return TRI_ERROR_NO_ERROR;
    }

    TRI_ASSERT(mptr.getDataPtr() != nullptr);
    
    buildDocumentIdentity(resultBuilder, cid, keyString, 
        mptr.revisionIdAsSlice(), VPackSlice(),
        nullptr, options.returnNew ? &mptr : nullptr);

    return TRI_ERROR_NO_ERROR;
  };

  int res;
  if (value.isArray()) {
    VPackArrayBuilder b(&resultBuilder);
    for (auto const s : VPackArrayIterator(value)) {
      res = workForOneDocument(s);
      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
  } else {
    res = workForOneDocument(value);
  }

  return OperationResult(resultBuilder.steal(), nullptr, "", res,
                         options.waitForSync); 
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief update/patch one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::update(std::string const& collectionName,
                                    VPackSlice const newValue,
                                    OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  OperationOptions optionsCopy = options;

  if (ServerState::instance()->isCoordinator()) {
    return updateCoordinator(collectionName, newValue, optionsCopy);
  }

  return modifyLocal(collectionName, newValue, optionsCopy,
                     TRI_VOC_DOCUMENT_OPERATION_UPDATE);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief update one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::updateCoordinator(std::string const& collectionName,
                                               VPackSlice const newValue,
                                               OperationOptions& options) {

  if (newValue.isArray()) {
    // multi-document variant is not yet implemented
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  
  auto headers = std::make_unique<std::map<std::string, std::string>>();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  std::string key(Transaction::extractKey(newValue));
  if (key.empty()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }
  TRI_voc_rid_t const expectedRevision 
      = options.ignoreRevs ? 0 : TRI_ExtractRevisionId(newValue);

  int res = arangodb::modifyDocumentOnCoordinator(
      _vocbase->_name, collectionName, key, expectedRevision,
      options.waitForSync, true /* isPatch */,
      options.keepNull, options.mergeObjects, newValue,
      headers, responseCode, resultHeaders, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    if (responseCode == arangodb::rest::HttpResponse::ACCEPTED ||
        responseCode == arangodb::rest::HttpResponse::CREATED ||
        responseCode == arangodb::rest::HttpResponse::PRECONDITION_FAILED) {
      VPackParser parser;
      try {
        parser.parse(resultBody);
        auto bui = parser.steal();
        auto buf = bui->steal();
        return OperationResult(buf, nullptr, "", 
            responseCode == arangodb::rest::HttpResponse::PRECONDITION_FAILED ?
            TRI_ERROR_ARANGO_CONFLICT : TRI_ERROR_NO_ERROR,
            responseCode == arangodb::rest::HttpResponse::CREATED);
      }
      catch (VPackException& e) {
        std::string message = "JSON from DBserver not parseable: "
                              + resultBody + ":" + e.what();
        return OperationResult(TRI_ERROR_INTERNAL, message);
      }
    } else if (responseCode == arangodb::rest::HttpResponse::BAD) {
      return OperationResult(TRI_ERROR_INTERNAL,
                             "JSON sent to DBserver was bad");
    } else if (responseCode == arangodb::rest::HttpResponse::NOT_FOUND) {
      return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    } else {
      return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief replace one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::replace(std::string const& collectionName,
                                     VPackSlice const newValue,
                                     OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  OperationOptions optionsCopy = options;

  if (ServerState::instance()->isCoordinator()) {
    return replaceCoordinator(collectionName, newValue, optionsCopy);
  }

  return modifyLocal(collectionName, newValue, optionsCopy,
                     TRI_VOC_DOCUMENT_OPERATION_REPLACE);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief replace one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::replaceCoordinator(std::string const& collectionName,
                                                VPackSlice const newValue,
                                                OperationOptions& options) {
  if (newValue.isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  auto headers = std::make_unique<std::map<std::string, std::string>>();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  std::string key(Transaction::extractKey(newValue));
  if (key.empty()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }
  TRI_voc_rid_t const expectedRevision 
      = options.ignoreRevs ? 0 : TRI_ExtractRevisionId(newValue);

  int res = arangodb::modifyDocumentOnCoordinator(
      _vocbase->_name, collectionName, key, expectedRevision,
      options.waitForSync, false /* isPatch */,
      false /* keepNull */, false /* mergeObjects */, newValue,
      headers, responseCode, resultHeaders, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    if (responseCode == arangodb::rest::HttpResponse::ACCEPTED ||
        responseCode == arangodb::rest::HttpResponse::CREATED ||
        responseCode == arangodb::rest::HttpResponse::PRECONDITION_FAILED) {
      VPackParser parser;
      try {
        parser.parse(resultBody);
        auto bui = parser.steal();
        auto buf = bui->steal();
        return OperationResult(buf, nullptr, "",
            responseCode == arangodb::rest::HttpResponse::PRECONDITION_FAILED ?
            TRI_ERROR_ARANGO_CONFLICT : TRI_ERROR_NO_ERROR,
            responseCode == arangodb::rest::HttpResponse::CREATED);
      }
      catch (VPackException& e) {
        std::string message = "JSON from DBserver not parseable: "
                              + resultBody + ":" + e.what();
        return OperationResult(TRI_ERROR_INTERNAL, message);
      }
    } else if (responseCode == arangodb::rest::HttpResponse::BAD) {
      return OperationResult(TRI_ERROR_INTERNAL,
                             "JSON sent to DBserver was bad");
    } else if (responseCode == arangodb::rest::HttpResponse::NOT_FOUND) {
      return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    } else {
      return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief replace one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::modifyLocal(
    std::string const& collectionName,
    VPackSlice const newValue,
    OperationOptions& options,
    TRI_voc_document_operation_e operation) {

  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));

  // Update/replace are a read and a write, let's get the write lock already
  // for the read operation:
  int res = lock(trxCollection(cid), TRI_TRANSACTION_WRITE);
  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  VPackBuilder resultBuilder;  // building the complete result

  auto workForOneDocument = [&](VPackSlice const newVal) -> int {
    if (!newVal.isObject()) {
      return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
    }
    TRI_doc_mptr_t mptr;
    VPackSlice actualRevision;

    if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
      res = document->replace(this, newVal, &mptr, options,
          !isLocked(document, TRI_TRANSACTION_WRITE), actualRevision);
    } else {
      res = document->update(this, newVal, &mptr, options,
          !isLocked(document, TRI_TRANSACTION_WRITE), actualRevision);
    }

    if (res == TRI_ERROR_ARANGO_CONFLICT) {
      // still return 
      std::string key = newVal.get(TRI_VOC_ATTRIBUTE_KEY).copyString();
      buildDocumentIdentity(resultBuilder, cid, key, actualRevision,
                            VPackSlice(), nullptr, nullptr);
      return TRI_ERROR_ARANGO_CONFLICT;
    } else if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_ASSERT(mptr.getDataPtr() != nullptr);

    if (!options.silent) {
      std::string key = newVal.get(TRI_VOC_ATTRIBUTE_KEY).copyString();
      buildDocumentIdentity(resultBuilder, cid, key, 
          mptr.revisionIdAsSlice(), actualRevision, nullptr, nullptr);
    }
    return TRI_ERROR_NO_ERROR;
  };

  res = TRI_ERROR_NO_ERROR;

  if (newValue.isArray()) {
    VPackArrayBuilder b(&resultBuilder);
    VPackArrayIterator it(newValue);
    while (it.valid()) {
      res = workForOneDocument(it.value());
      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
      ++it;
    }
  } else {
    res = workForOneDocument(newValue);
  }
  return OperationResult(resultBuilder.steal(), nullptr, "", res,
                         options.waitForSync); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief remove one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::remove(std::string const& collectionName,
                                    VPackSlice const value,
                                    OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!value.isObject() && !value.isArray() && !value.isString()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  OperationOptions optionsCopy = options;

  if (ServerState::instance()->isCoordinator()) {
    return removeCoordinator(collectionName, value, optionsCopy);
  }

  return removeLocal(collectionName, value, optionsCopy);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief remove one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::removeCoordinator(std::string const& collectionName,
                                               VPackSlice const value,
                                               OperationOptions& options) {

  if (value.isArray()) {
    // multi-document variant is not yet implemented
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  
  auto headers = std::make_unique<std::map<std::string, std::string>>();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  std::string key(Transaction::extractKey(value));
  if (key.empty()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }
  TRI_voc_rid_t expectedRevision = TRI_ExtractRevisionId(value);

  int res = arangodb::deleteDocumentOnCoordinator(
      _vocbase->_name, collectionName, key, expectedRevision,
      options.waitForSync,
      headers, responseCode, resultHeaders, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    if (responseCode == arangodb::rest::HttpResponse::OK ||
        responseCode == arangodb::rest::HttpResponse::ACCEPTED ||
        responseCode == arangodb::rest::HttpResponse::PRECONDITION_FAILED) {
      VPackParser parser;
      try {
        parser.parse(resultBody);
        auto bui = parser.steal();
        auto buf = bui->steal();
        return OperationResult(buf, nullptr, "", TRI_ERROR_NO_ERROR, true);
      }
      catch (VPackException& e) {
        std::string message = "JSON from DBserver not parseable: "
                              + resultBody + ":" + e.what();
        return OperationResult(TRI_ERROR_INTERNAL, message);
      }
    } else if (responseCode == arangodb::rest::HttpResponse::BAD) {
      return OperationResult(TRI_ERROR_INTERNAL,
                             "JSON sent to DBserver was bad");
    } else if (responseCode == arangodb::rest::HttpResponse::NOT_FOUND) {
      return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    } else {
      return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief remove one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::removeLocal(std::string const& collectionName,
                                         VPackSlice const value,
                                         OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));
 
  VPackBuilder resultBuilder;

  auto workOnOneDocument = [&](VPackSlice const value) -> int {
    VPackSlice actualRevision;
    int res = document->remove(this, value, options,
                               !isLocked(document, TRI_TRANSACTION_WRITE),
                               actualRevision);
    
    if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_CONFLICT) {
        std::string key = value.get(TRI_VOC_ATTRIBUTE_KEY).copyString();
        buildDocumentIdentity(resultBuilder, cid, key,
                              actualRevision, VPackSlice(), nullptr, nullptr);
      }
      return res;
    }

    if (options.silent) {
      return TRI_ERROR_NO_ERROR;
    }

    std::string key = value.get(TRI_VOC_ATTRIBUTE_KEY).copyString();
    buildDocumentIdentity(resultBuilder, cid, key,
                          actualRevision, VPackSlice(), nullptr, nullptr);

    return TRI_ERROR_NO_ERROR;
  };

  int res = TRI_ERROR_NO_ERROR;
  if (value.isArray()) {
    VPackArrayBuilder guard(&resultBuilder);
    for (auto const s : VPackArrayIterator(value)) {
      res = workOnOneDocument(s);
      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
    }
  } else {
    res = workOnOneDocument(value);
  }
  return OperationResult(resultBuilder.steal(), nullptr, "", res,
                         options.waitForSync); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches all document keys in a collection
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::allKeys(std::string const& collectionName,
                                     std::string const& type,
                                     OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  
  std::string prefix;

  std::string realCollName = resolver()->getCollectionName(collectionName);

  if (type == "key") {
    prefix = "";
  } else if (type == "id") {
    prefix = realCollName + "/";
  } else {
    // default return type: paths to documents
    if (isEdgeCollection(collectionName)) {
      prefix = std::string("/_db/") + _vocbase->_name + "/_api/edge/" + realCollName + "/";
    } else {
      prefix = std::string("/_db/") + _vocbase->_name + "/_api/document/" + realCollName + "/";
    }
  }
  
  OperationOptions optionsCopy = options;

  if (ServerState::instance()->isCoordinator()) {
    return allKeysCoordinator(collectionName, type, prefix, optionsCopy);
  }

  return allKeysLocal(collectionName, type, prefix, optionsCopy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches all document keys in a collection, coordinator
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::allKeysCoordinator(std::string const& collectionName,
                                                std::string const& type,
                                                std::string const& prefix,
                                                OperationOptions& options) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches all document keys in a collection, local
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::allKeysLocal(std::string const& collectionName,
                                          std::string const& type,
                                          std::string const& prefix,
                                          OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  orderDitch(cid); // will throw when it fails
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
  
  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(VPackValueType::Object));
  resultBuilder.add("documents", VPackValue(VPackValueType::Array));
  
  OperationCursor cursor = indexScan(collectionName, Transaction::CursorType::ALL, "", {}, 0, UINT64_MAX, 1000, false);

  while (cursor.hasMore()) {
    int res = cursor.getMore();

    if (res != TRI_ERROR_NO_ERROR) {
      return OperationResult(res);
    }
  
    std::string value;
    VPackSlice docs = cursor.slice();
    VPackArrayIterator it(docs);
    while (it.valid()) {
      value.assign(prefix);
      value.append(it.value().get(TRI_VOC_ATTRIBUTE_KEY).copyString());
      resultBuilder.add(VPackValue(value));
      it.next();
    }
  }

  resultBuilder.close(); // array
  resultBuilder.close(); // object

  res = unlock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationCursor(res);
  }

  return OperationResult(resultBuilder.steal(),
                         transactionContext()->orderCustomTypeHandler(), "",
                         TRI_ERROR_NO_ERROR, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches all documents in a collection
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::all(std::string const& collectionName,
                                 uint64_t skip, uint64_t limit,
                                 OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  
  OperationOptions optionsCopy = options;

  if (ServerState::instance()->isCoordinator()) {
    return allCoordinator(collectionName, skip, limit, optionsCopy);
  }

  return allLocal(collectionName, skip, limit, optionsCopy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches all documents in a collection, coordinator
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::allCoordinator(std::string const& collectionName,
                                            uint64_t skip, uint64_t limit, 
                                            OperationOptions& options) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches all documents in a collection, local
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::allLocal(std::string const& collectionName,
                                      uint64_t skip, uint64_t limit,
                                      OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  orderDitch(cid); // will throw when it fails
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
  
  VPackBuilder resultBuilder;
  resultBuilder.openArray();
  
  OperationCursor cursor = indexScan(collectionName, Transaction::CursorType::ALL, "", {}, skip, limit, 1000, false);

  while (cursor.hasMore()) {
    int res = cursor.getMore();

    if (res != TRI_ERROR_NO_ERROR) {
      return OperationResult(res);
    }
  
    VPackSlice docs = cursor.slice();
    VPackArrayIterator it(docs);
    while (it.valid()) {
      resultBuilder.add(it.value());
      it.next();
    }
  }

  resultBuilder.close();

  res = unlock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationCursor(res);
  }

  return OperationResult(resultBuilder.steal(),
                         transactionContext()->orderCustomTypeHandler(), "",
                         TRI_ERROR_NO_ERROR, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all documents in a collection
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::truncate(std::string const& collectionName,
                                      OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  
  OperationOptions optionsCopy = options;

  if (ServerState::instance()->isCoordinator()) {
    return truncateCoordinator(collectionName, optionsCopy);
  }

  return truncateLocal(collectionName, optionsCopy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all documents in a collection, coordinator
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::truncateCoordinator(std::string const& collectionName,
                                                 OperationOptions& options) {
  return OperationResult(
      arangodb::truncateCollectionOnCoordinator(_vocbase->_name,
                                                collectionName));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all documents in a collection, local
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::truncateLocal(std::string const& collectionName,
                                           OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  orderDitch(cid); // will throw when it fails
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_WRITE);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
 
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));
  
  VPackBuilder keyBuilder;
  auto primaryIndex = document->primaryIndex();

  options.ignoreRevs = true;

  auto callback = [&](TRI_doc_mptr_t const* mptr) {
    VPackSlice actualRevision;
    int res = document->remove(this, VPackSlice(mptr->vpack()), options, false,
                               actualRevision);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    return true;
  };

  try {
    primaryIndex->invokeOnAllElementsForRemoval(callback);
  }
  catch (basics::Exception const& ex) {
    unlock(trxCollection(cid), TRI_TRANSACTION_WRITE);
    return OperationResult(ex.code());
  }
  
  res = unlock(trxCollection(cid), TRI_TRANSACTION_WRITE);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationCursor(res);
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief count the number of documents in a collection
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::count(std::string const& collectionName) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (ServerState::instance()->isCoordinator()) {
    return countCoordinator(collectionName);
  }

  return countLocal(collectionName);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief count the number of documents in a collection
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::countCoordinator(std::string const& collectionName) {
  uint64_t count = 0;
  int res = arangodb::countOnCoordinator(_vocbase->_name, collectionName, count);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(count));

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR, false);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief count the number of documents in a collection
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::countLocal(std::string const& collectionName) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
  
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));

  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(document->size()));

  res = unlock(trxCollection(cid), TRI_TRANSACTION_READ);
  
  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR, false);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief factory for OperationCursor objects
/// note: the caller must have read-locked the underlying collection when
/// calling this method
//////////////////////////////////////////////////////////////////////////////

OperationCursor Transaction::indexScan(
    std::string const& collectionName, CursorType cursorType,
    std::string const& indexId, VPackSlice const search,
    uint64_t skip, uint64_t limit, uint64_t batchSize, bool reverse) {

#warning TODO Who checks if indexId is valid and is used for this collection?
  // For now we assume indexId is the iid part of the index.

  if (ServerState::instance()->isCoordinator()) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationCursor(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  if (limit == 0) {
    // nothing to do
    return OperationCursor(TRI_ERROR_NO_ERROR);
  }

  TRI_document_collection_t* document = documentCollection(trxCollection(cid));

  std::unique_ptr<IndexIterator> iterator;

  switch (cursorType) {
    case CursorType::ANY: {
      // We do not need search values
      TRI_ASSERT(search.isNone());
      // We do not need an index either
      TRI_ASSERT(indexId.empty());

      arangodb::PrimaryIndex* idx = document->primaryIndex();

      if (idx == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
            "Could not find primary index in collection '" + collectionName + "'.");
      }

      iterator.reset(idx->anyIterator(this));
      break;
    }
    case CursorType::ALL: {
      // We do not need search values
      TRI_ASSERT(search.isNone());
      // We do not need an index either
      TRI_ASSERT(indexId.empty());

      arangodb::PrimaryIndex* idx = document->primaryIndex();

      if (idx == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
            "Could not find primary index in collection '" + collectionName + "'.");
      }

      iterator.reset(idx->allIterator(this, reverse));
      break;
    }
    case CursorType::INDEX: {
      if (indexId.empty()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "The index id cannot be empty.");
      }
      arangodb::Index* idx = nullptr;

      if (!arangodb::Index::validateId(indexId.c_str())) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
      }
      TRI_idx_iid_t iid = arangodb::basics::StringUtils::uint64(indexId);
      idx = document->lookupIndex(iid);

      if (idx == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
                                       "Could not find index '" + indexId +
                                           "' in collection '" +
                                           collectionName + "'.");
      }
      
      // We have successfully found an index with the requested id.
      
      // Normalize the search values
      VPackBuilder expander;
      idx->expandInSearchValues(search, expander);

      // Now collect the Iterator
      IndexIteratorContext ctxt(_vocbase, resolver());
      iterator.reset(idx->iteratorForSlice(this, &ctxt, expander.slice(), reverse));
    }
  }
  if (iterator == nullptr) {
    // We could not create an ITERATOR and it did not throw an error itself
    return OperationCursor(TRI_ERROR_OUT_OF_MEMORY);
  }

  uint64_t unused = 0;
  iterator->skip(skip, unused);

  return OperationCursor(transactionContext()->orderCustomTypeHandler(),
                         iterator.release(), limit, batchSize);
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* Transaction::documentCollection(
      TRI_transaction_collection_t const* trxCollection) const {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  TRI_ASSERT(trxCollection->_collection != nullptr);
  TRI_ASSERT(trxCollection->_collection->_collection != nullptr);

  return trxCollection->_collection->_collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* Transaction::documentCollection(
      TRI_voc_cid_t cid) const {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  
  auto trxCollection = TRI_GetCollectionTransaction(_trx, cid, TRI_TRANSACTION_READ);
  TRI_ASSERT(trxCollection->_collection != nullptr);
  TRI_ASSERT(trxCollection->_collection->_collection != nullptr);

  return trxCollection->_collection->_collection;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by id, with the name supplied
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollection(TRI_voc_cid_t cid, char const* name,
                    TRI_transaction_type_e type) {
  int res = this->addCollection(cid, type);

  if (res != TRI_ERROR_NO_ERROR) {
    _errorData = std::string(name);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by id, with the name supplied
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollection(TRI_voc_cid_t cid, std::string const& name,
                    TRI_transaction_type_e type) {
  return addCollection(cid, name.c_str(), type);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by id
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollection(TRI_voc_cid_t cid, TRI_transaction_type_e type) {
  if (_trx == nullptr) {
    return registerError(TRI_ERROR_INTERNAL);
  }

  if (cid == 0) {
    // invalid cid
    return registerError(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  if (_setupState != TRI_ERROR_NO_ERROR) {
    return _setupState;
  }

  TRI_transaction_status_e const status = getStatus();

  if (status == TRI_TRANSACTION_COMMITTED ||
      status == TRI_TRANSACTION_ABORTED) {
    // transaction already finished?
    return registerError(TRI_ERROR_TRANSACTION_INTERNAL);
  }

  if (this->isEmbeddedTransaction()) {
   return this->addCollectionEmbedded(cid, type);
  } 

  return this->addCollectionToplevel(cid, type);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by name
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollection(std::string const& name, TRI_transaction_type_e type) {
  if (_setupState != TRI_ERROR_NO_ERROR) {
    return _setupState;
  }

  return addCollection(this->resolver()->getCollectionId(name),
                       name.c_str(), type);
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief test if a collection is already locked
////////////////////////////////////////////////////////////////////////////////

bool Transaction::isLocked(TRI_transaction_collection_t const* trxCollection,
                TRI_transaction_type_e type) {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    return false;
  }

  return TRI_IsLockedCollectionTransaction(trxCollection, type,
                                           _nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if a collection is already locked
////////////////////////////////////////////////////////////////////////////////

bool Transaction::isLocked(TRI_document_collection_t* document,
                TRI_transaction_type_e type) {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    return false;
  }

  TRI_transaction_collection_t* trxCollection =
      TRI_GetCollectionTransaction(_trx, document->_info.id(), type);
  return isLocked(trxCollection, type);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-lock a collection
////////////////////////////////////////////////////////////////////////////////

int Transaction::lock(TRI_transaction_collection_t* trxCollection,
           TRI_transaction_type_e type) {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  return TRI_LockCollectionTransaction(trxCollection, type, _nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-unlock a collection
////////////////////////////////////////////////////////////////////////////////

int Transaction::unlock(TRI_transaction_collection_t* trxCollection,
             TRI_transaction_type_e type) {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  return TRI_UnlockCollectionTransaction(trxCollection, type, _nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to an embedded transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollectionEmbedded(TRI_voc_cid_t cid, TRI_transaction_type_e type) {
  TRI_ASSERT(_trx != nullptr);

  int res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel,
                                         false, _allowImplicitCollections);

  if (res != TRI_ERROR_NO_ERROR) {
    return registerError(res);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a top-level transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollectionToplevel(TRI_voc_cid_t cid, TRI_transaction_type_e type) {
  TRI_ASSERT(_trx != nullptr);

  int res;

  if (getStatus() != TRI_TRANSACTION_CREATED) {
    // transaction already started?
    res = TRI_ERROR_TRANSACTION_INTERNAL;
  } else {
    res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel, false,
                                       _allowImplicitCollections);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    registerError(res);
  }

  return res;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the transaction
/// this will first check if the transaction is embedded in a parent
/// transaction. if not, it will create a transaction of its own
////////////////////////////////////////////////////////////////////////////////

int Transaction::setupTransaction() {
  // check in the context if we are running embedded
  _trx = this->_transactionContext->getParentTransaction();

  if (_trx != nullptr) {
    // yes, we are embedded
    _setupState = setupEmbedded();
  } else {
    // non-embedded
    _setupState = setupToplevel();
  }

  // this may well be TRI_ERROR_NO_ERROR...
  return _setupState;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief set up an embedded transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::setupEmbedded() {
  TRI_ASSERT(_nestingLevel == 0);

  _nestingLevel = ++_trx->_nestingLevel;

  if (!this->_transactionContext->isEmbeddable()) {
    // we are embedded but this is disallowed...
    return TRI_ERROR_TRANSACTION_NESTED;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up a top-level transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::setupToplevel() {
  TRI_ASSERT(_nestingLevel == 0);

  // we are not embedded. now start our own transaction
  _trx = TRI_CreateTransaction(_vocbase, _externalId, _timeout, _waitForSync);

  if (_trx == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // register the transaction in the context
  return this->_transactionContext->registerTransaction(_trx);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free transaction
////////////////////////////////////////////////////////////////////////////////

void Transaction::freeTransaction() {
  TRI_ASSERT(!isEmbeddedTransaction());

  if (_trx != nullptr) {
    auto id = _trx->_id;
    bool hasFailedOperations = TRI_FreeTransaction(_trx);
    _trx = nullptr;
      
    // store result
    this->_transactionContext->storeTransactionResult(id, hasFailedOperations);
    this->_transactionContext->unregisterTransaction();
  }
}


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

#include "Utils/transactions.h"
#include "Basics/conversions.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/PrimaryIndex.h"
#include "Utils/OperationCursor.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/MasterPointers.h"

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

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _key attribute from a slice
////////////////////////////////////////////////////////////////////////////////

std::string Transaction::extractKey(VPackSlice const* slice) {
  TRI_ASSERT(slice != nullptr);
  
  // extract _key
  if (slice->isObject()) {
    VPackSlice k = slice->get(TRI_VOC_ATTRIBUTE_KEY);
    if (!k.isString()) {
      return ""; // fail
    }
    return k.copyString();
  } 
  if (slice->isString()) {
    return slice->copyString();
  } 
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _rev attribute from a slice
////////////////////////////////////////////////////////////////////////////////

TRI_voc_rid_t Transaction::extractRevisionId(VPackSlice const* slice) {
  TRI_ASSERT(slice != nullptr);
  TRI_ASSERT(slice->isObject());

  VPackSlice r(slice->get(TRI_VOC_ATTRIBUTE_REV));
  if (r.isString()) {
    VPackValueLength length;
    char const* p = r.getString(length);
    return arangodb::basics::StringUtils::uint64(p, length);
  }
  if (r.isInteger()) {
    return r.getNumber<TRI_voc_rid_t>();
  }
  return 0;
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief build a VPack object with _id, _key and _rev, the result is
/// added to the builder in the argument as a single object.
//////////////////////////////////////////////////////////////////////////////

void Transaction::buildDocumentIdentity(VPackBuilder& builder,
                                        TRI_voc_cid_t cid,
                                        std::string const& key,
                                        std::string const& rid,
                                        std::string const& oldRid) {
  std::string collectionName = resolver()->getCollectionName(cid);

  builder.openObject();
  builder.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(collectionName + "/" + key));
  builder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
  builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(rid));
  if (!oldRid.empty()) {
    builder.add("_oldRev", VPackValue(oldRid));
  }
  builder.close();
}

void Transaction::buildDocumentIdentity(VPackBuilder& builder,
                                        TRI_voc_cid_t cid,
                                        std::string const& key,
                                        TRI_voc_rid_t rid,
                                        std::string const& oldRid) {
  std::string ridSt = std::to_string(rid);
  buildDocumentIdentity(builder, cid, key, ridSt, oldRid);
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
/// @brief read all master pointers, using skip and limit and an internal
/// offset into the primary index. this can be used for incremental access to
/// the documents without restarting the index scan at the begin
////////////////////////////////////////////////////////////////////////////////

int Transaction::readIncremental(TRI_transaction_collection_t* trxCollection,
                                 std::vector<TRI_doc_mptr_t>& docs,
                                 arangodb::basics::BucketPosition& internalSkip,
                                 uint64_t batchSize, uint64_t& skip,
                                 uint64_t limit, uint64_t& total) {
  TRI_document_collection_t* document = documentCollection(trxCollection);

  // READ-LOCK START
  int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (orderDitch(trxCollection) == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  try {
    if (batchSize > 2048) {
      docs.reserve(2048);
    } else if (batchSize > 0) {
      docs.reserve(batchSize);
    }

    auto primaryIndex = document->primaryIndex();
    uint64_t count = 0;

    while (count < batchSize || skip > 0) {
      TRI_doc_mptr_t const* mptr =
          primaryIndex->lookupSequential(this, internalSkip, total);

      if (mptr == nullptr) {
        break;
      }
      if (skip > 0) {
        --skip;
      } else {
        docs.emplace_back(*mptr);

        if (++count >= limit) {
          break;
        }
      }
    }
  } catch (...) {
    this->unlock(trxCollection, TRI_TRANSACTION_READ);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  this->unlock(trxCollection, TRI_TRANSACTION_READ);
  // READ-LOCK END

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers, using skip and limit and an internal
/// offset into the primary index. this can be used for incremental access to
/// the documents without restarting the index scan at the begin
////////////////////////////////////////////////////////////////////////////////

int Transaction::any(TRI_transaction_collection_t* trxCollection,
                     std::vector<TRI_doc_mptr_t>& docs,
                     arangodb::basics::BucketPosition& initialPosition,
                     arangodb::basics::BucketPosition& position,
                     uint64_t batchSize, uint64_t& step,
                     uint64_t& total) {
  TRI_document_collection_t* document = documentCollection(trxCollection);
  // READ-LOCK START
  int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  if (orderDitch(trxCollection) == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  uint64_t numRead = 0;
  TRI_ASSERT(batchSize > 0);

  while (numRead < batchSize) {
    auto mptr = document->primaryIndex()->lookupRandom(this, initialPosition,
                                                       position, step, total);
    if (mptr == nullptr) {
      // Read all documents randomly
      break;
    }
    docs.emplace_back(*mptr);
    ++numRead;
  }
  this->unlock(trxCollection, TRI_TRANSACTION_READ);
  // READ-LOCK END
  return TRI_ERROR_NO_ERROR;
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
  
  if (orderDitch(trxCollection(cid)) == nullptr) {
    return OperationResult(TRI_ERROR_OUT_OF_MEMORY);
  }
  
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
/// @brief return one or multiple documents from a collection
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::document(std::string const& collectionName,
                                      VPackSlice const& value,
                                      OperationOptions& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (value.isArray()) {
    // multi-document variant is not yet implemented
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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
                                                 VPackSlice const& value,
                                                 OperationOptions& options) {
  auto headers = std::make_unique<std::map<std::string, std::string>>();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  std::string key(Transaction::extractKey(&value));
  if (key.empty()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }
  TRI_voc_rid_t expectedRevision = Transaction::extractRevisionId(&value);

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
                                           VPackSlice const& value,
                                           OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  std::string key(Transaction::extractKey(&value));
  if (key.empty()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }

  TRI_voc_rid_t expectedRevision = Transaction::extractRevisionId(&value);

  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));

  if (orderDitch(trxCollection(cid)) == nullptr) {
    return OperationResult(TRI_ERROR_OUT_OF_MEMORY);
  }
 
  TRI_doc_mptr_t mptr;
  int res = document->read(this, key, &mptr, !isLocked(document, TRI_TRANSACTION_READ));

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
  
  TRI_ASSERT(mptr.getDataPtr() != nullptr);
  if (expectedRevision != 0 && expectedRevision != mptr.revisionId()) {
    // still return 
    VPackBuilder resultBuilder;
    buildDocumentIdentity(resultBuilder, cid, key, mptr.revisionId(), "");

    return OperationResult(resultBuilder.steal(), nullptr, "",
        TRI_ERROR_ARANGO_CONFLICT,
        options.waitForSync || document->_info.waitForSync()); 
  }
  
  VPackBuilder resultBuilder;
  if (!options.silent) {
    resultBuilder.add(VPackSlice(mptr.vpack()));
  }

  return OperationResult(resultBuilder.steal(), 
                         transactionContext()->orderCustomTypeHandler(), "",
                         TRI_ERROR_NO_ERROR, false); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief create one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::insert(std::string const& collectionName,
                                    VPackSlice const& value,
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
      std::string cName = docId.substr(0, split);
      if (TRI_COL_TYPE_UNKNOWN == resolver()->getCollectionType(cName)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
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
      std::string cName = docId.substr(0, split);
      if (TRI_COL_TYPE_UNKNOWN == resolver()->getCollectionType(cName)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
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
                                               VPackSlice const& value,
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
                                         VPackSlice const& value,
                                         OperationOptions& options) {
 
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  TRI_document_collection_t* document = documentCollection(trxCollection(cid));

  VPackBuilder resultBuilder;

  auto workForOneDocument = [&](VPackSlice const value) -> int {
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
    int res = document->insert(this, &insertSlice, &mptr, options,
        !isLocked(document, TRI_TRANSACTION_WRITE));
    
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (options.silent) {
      // no need to construct the result object
      return TRI_ERROR_NO_ERROR;
    }

    TRI_ASSERT(mptr.getDataPtr() != nullptr);
    
    buildDocumentIdentity(resultBuilder, cid, keyString, mptr.revisionId(), "");
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
                                    VPackSlice const& oldValue,
                                    VPackSlice const& newValue,
                                    OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!oldValue.isObject() && !oldValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  
  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (oldValue.isArray() || newValue.isArray()) {
    // multi-document variant is not yet implemented
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  
  OperationOptions optionsCopy = options;

  if (ServerState::instance()->isCoordinator()) {
    return updateCoordinator(collectionName, oldValue, newValue, optionsCopy);
  }

  return updateLocal(collectionName, oldValue, newValue, optionsCopy);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief update one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::updateCoordinator(std::string const& collectionName,
                                               VPackSlice const& oldValue,
                                               VPackSlice const& newValue,
                                               OperationOptions& options) {
  auto headers = std::make_unique<std::map<std::string, std::string>>();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  std::string key(Transaction::extractKey(&oldValue));
  if (key.empty()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }
  TRI_voc_rid_t expectedRevision = Transaction::extractRevisionId(&oldValue);

  int res = arangodb::modifyDocumentOnCoordinator(
      _vocbase->_name, collectionName, key, expectedRevision,
      TRI_DOC_UPDATE_ERROR, options.waitForSync, true /* isPatch */,
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
/// @brief update one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::updateLocal(std::string const& collectionName,
                                         VPackSlice const& oldValue,
                                         VPackSlice const& newValue,
                                         OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  // read expected revision
  std::string const key(Transaction::extractKey(&oldValue));
  TRI_voc_rid_t const expectedRevision = Transaction::extractRevisionId(&oldValue);

  // generate a new tick value
  TRI_voc_tick_t const revisionId = TRI_NewTickServer();
  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));


  VPackBuilder builder;
  builder.openObject();

  VPackObjectIterator it(newValue);
  while (it.valid()) {
    // let all but the system attributes pass
    std::string k = it.key().copyString();
    if (k[0] != '_' ||
        (k != TRI_VOC_ATTRIBUTE_KEY &&
         k != TRI_VOC_ATTRIBUTE_ID &&
         k != TRI_VOC_ATTRIBUTE_REV &&
         k != TRI_VOC_ATTRIBUTE_FROM &&
         k != TRI_VOC_ATTRIBUTE_TO)) {
      builder.add(k, it.value());
    }
    it.next();
  }
  // finally add (new) _rev attribute
  builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(std::to_string(revisionId)));
  builder.close();

  VPackSlice sanitized = builder.slice();

  if (orderDitch(trxCollection(cid)) == nullptr) {
    return OperationResult(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  TRI_doc_mptr_t mptr;
  TRI_voc_rid_t actualRevision = 0;
  TRI_doc_update_policy_t policy(expectedRevision == 0 ? TRI_DOC_UPDATE_LAST_WRITE : TRI_DOC_UPDATE_ERROR, expectedRevision, &actualRevision);

  int res = lock(trxCollection(cid), TRI_TRANSACTION_WRITE);
  
  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  res = document->update(this, &oldValue, &sanitized, &mptr, &policy, options, !isLocked(document, TRI_TRANSACTION_WRITE));

  if (res == TRI_ERROR_ARANGO_CONFLICT) {
    // still return 
    VPackBuilder resultBuilder;
    buildDocumentIdentity(resultBuilder, cid, key, mptr.revisionId(), "");

    return OperationResult(resultBuilder.steal(), nullptr, "",
        TRI_ERROR_ARANGO_CONFLICT,
        options.waitForSync || document->_info.waitForSync()); 
  } else if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);

  if (options.silent) {
    return OperationResult(TRI_ERROR_NO_ERROR);
  }

  VPackBuilder resultBuilder;
  buildDocumentIdentity(resultBuilder, cid, key, std::to_string(revisionId), std::to_string(actualRevision));

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR,
                         options.waitForSync); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief replace one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::replace(std::string const& collectionName,
                                     VPackSlice const& oldValue,
                                     VPackSlice const& newValue,
                                     OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!oldValue.isObject() && !oldValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  
  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if ((!oldValue.isArray() ^ !newValue.isArray()) ||
      (oldValue.isArray() && oldValue.length() != newValue.length())) {
    // must provide two lists or two single documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  
  OperationOptions optionsCopy = options;

  if (ServerState::instance()->isCoordinator()) {
    return replaceCoordinator(collectionName, oldValue, newValue, optionsCopy);
  }

  return replaceLocal(collectionName, oldValue, newValue, optionsCopy);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief replace one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::replaceCoordinator(std::string const& collectionName,
                                                VPackSlice const& oldValue,
                                                VPackSlice const& newValue,
                                                OperationOptions& options) {
  if (oldValue.isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  auto headers = std::make_unique<std::map<std::string, std::string>>();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  std::string key(Transaction::extractKey(&oldValue));
  if (key.empty()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }
  TRI_voc_rid_t expectedRevision = Transaction::extractRevisionId(&oldValue);

  int res = arangodb::modifyDocumentOnCoordinator(
      _vocbase->_name, collectionName, key, expectedRevision,
      TRI_DOC_UPDATE_ERROR, options.waitForSync, false /* isPatch */,
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

OperationResult Transaction::replaceLocal(std::string const& collectionName,
                                          VPackSlice const& oldValue,
                                          VPackSlice const& newValue,
                                          OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));

  // Replace is a read and a write, let's get the write lock already
  // for the read operation:
  int res = lock(trxCollection(cid), TRI_TRANSACTION_WRITE);
  
  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  VPackBuilder builder;        // used repeatedly in closure
  VPackBuilder resultBuilder;  // building the complete result

  auto workForOneDocument = [&](VPackSlice const oldVal,
                                VPackSlice const newVal) -> int {

    // read key and expected revision
    std::string const key(Transaction::extractKey(&oldVal));
    TRI_voc_rid_t const expectedRevision 
        = Transaction::extractRevisionId(&oldVal);

    // generate a new tick value
    TRI_voc_tick_t const revisionId = TRI_NewTickServer();

    builder.clear();
    {
      VPackObjectBuilder b(&builder);
      VPackObjectIterator it(newValue);
      while (it.valid()) {
        // let all but the system attributes pass
        std::string k = it.key().copyString();
        if (k[0] != '_' ||
            (k != TRI_VOC_ATTRIBUTE_KEY &&
             k != TRI_VOC_ATTRIBUTE_ID &&
             k != TRI_VOC_ATTRIBUTE_REV &&
             k != TRI_VOC_ATTRIBUTE_FROM &&
             k != TRI_VOC_ATTRIBUTE_TO)) {
          builder.add(k, it.value());
        }
        it.next();
      }
      // finally add the (new) _rev attributes
      builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(std::to_string(revisionId)));
    }

    VPackSlice sanitized = builder.slice();

    TRI_doc_mptr_t mptr;
    TRI_voc_rid_t actualRevision = 0;
    TRI_doc_update_policy_t policy(expectedRevision == 0 ? 
                                   TRI_DOC_UPDATE_LAST_WRITE :
                                   TRI_DOC_UPDATE_ERROR, 
                                   expectedRevision, &actualRevision);

    res = document->replace(this, &oldValue, &sanitized, &mptr, &policy,
        options, !isLocked(document, TRI_TRANSACTION_WRITE));

    if (res == TRI_ERROR_ARANGO_CONFLICT) {
      // still return 
      buildDocumentIdentity(resultBuilder, cid, key, mptr.revisionId(), "");
      return TRI_ERROR_ARANGO_CONFLICT;
    } else if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_ASSERT(mptr.getDataPtr() != nullptr);

    if (!options.silent) {
      buildDocumentIdentity(resultBuilder, cid, key, std::to_string(revisionId), std::to_string(actualRevision));
    }
    return TRI_ERROR_NO_ERROR;
  };

  res = TRI_ERROR_NO_ERROR;

  if (oldValue.isArray()) {
    // We already know that both oldValue and newValue are arrays and that
    // they have equal length!
    VPackArrayBuilder b(&resultBuilder);
    VPackArrayIterator oIt(oldValue);
    VPackArrayIterator nIt(newValue);
    while (oIt.valid() && nIt.valid()) {
      res = workForOneDocument(oIt.value(), nIt.value());
      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }
      ++oIt;
      ++nIt;
    }
  } else {
    res = workForOneDocument(oldValue, newValue);
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
                                    VPackSlice const& value,
                                    OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!value.isObject() && !value.isArray() && !value.isString()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (value.isArray()) {
    // multi-document variant is not yet implemented
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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
                                               VPackSlice const& value,
                                               OperationOptions& options) {
  auto headers = std::make_unique<std::map<std::string, std::string>>();
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  std::string key(Transaction::extractKey(&value));
  if (key.empty()) {
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }
  TRI_voc_rid_t expectedRevision = Transaction::extractRevisionId(&value);

  int res = arangodb::deleteDocumentOnCoordinator(
      _vocbase->_name, collectionName, key, expectedRevision,
      TRI_DOC_UPDATE_ERROR, options.waitForSync,
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
                                         VPackSlice const& value,
                                         OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));
 
  std::string key; 
  TRI_voc_rid_t const expectedRevision = Transaction::extractRevisionId(&value);

  VPackBuilder builder;
  builder.openObject();

  // extract _key
  if (value.isObject()) {
    VPackSlice k = value.get(TRI_VOC_ATTRIBUTE_KEY);
    if (!k.isString()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    }
    builder.add(TRI_VOC_ATTRIBUTE_KEY, k);
    key = k.copyString();
  } else if (value.isString()) {
    builder.add(TRI_VOC_ATTRIBUTE_KEY, value);
    key = value.copyString();
  }
  
  // add _rev  
  builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(std::to_string(expectedRevision)));
  builder.close();

  VPackSlice removeSlice = builder.slice();

  TRI_voc_rid_t actualRevision = 0;
  TRI_doc_update_policy_t updatePolicy(expectedRevision == 0 ? TRI_DOC_UPDATE_LAST_WRITE : TRI_DOC_UPDATE_ERROR, expectedRevision, &actualRevision);
  int res = document->remove(this, &removeSlice, &updatePolicy, options, !isLocked(document, TRI_TRANSACTION_WRITE));
  
  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  if (options.silent) {
    return OperationResult(TRI_ERROR_NO_ERROR);
  }

  VPackBuilder resultBuilder;
  buildDocumentIdentity(resultBuilder, cid, key, std::to_string(actualRevision), "");

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR,
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

  if (type == "key") {
    prefix = "";
  } else if (type == "id") {
    prefix = collectionName + "/";
  } else {
    // default return type: paths to documents
    if (isEdgeCollection(collectionName)) {
      prefix = std::string("/_db/") + _vocbase->_name + "/_api/edge/" + collectionName + "/";
    } else {
      prefix = std::string("/_db/") + _vocbase->_name + "/_api/document/" + collectionName + "/";
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
  
  if (orderDitch(trxCollection(cid)) == nullptr) {
    return OperationResult(TRI_ERROR_OUT_OF_MEMORY);
  }
  
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
  
  if (orderDitch(trxCollection(cid)) == nullptr) {
    return OperationResult(TRI_ERROR_OUT_OF_MEMORY);
  }
  
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
  
  if (orderDitch(trxCollection(cid)) == nullptr) {
    return OperationResult(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_WRITE);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
 
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));
  
  TRI_voc_rid_t actualRevision = 0;
  TRI_doc_update_policy_t updatePolicy(TRI_DOC_UPDATE_LAST_WRITE, 0, &actualRevision);
  
  VPackBuilder keyBuilder;
  auto primaryIndex = document->primaryIndex();

  std::function<void(TRI_doc_mptr_t*)> callback = [this, &document, &keyBuilder, &updatePolicy, &options](TRI_doc_mptr_t const* mptr) {
    VPackSlice slice(mptr->vpack());
    VPackSlice keySlice = slice.get(TRI_VOC_ATTRIBUTE_KEY);

    keyBuilder.clear();
    keyBuilder.openObject();
    keyBuilder.add(TRI_VOC_ATTRIBUTE_KEY, keySlice);
    keyBuilder.close();

    VPackSlice builderSlice = keyBuilder.slice();

    int res = document->remove(this, &builderSlice, &updatePolicy, options, false);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
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

  // TODO Who checks if indexId is valid and is used for this collection?
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
      TRI_idx_iid_t iid = arangodb::basics::StringUtils::uint64(indexId.c_str(), indexId.size());
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

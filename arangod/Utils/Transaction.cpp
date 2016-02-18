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
#include "Indexes/PrimaryIndex.h"
#include "Storage/Marker.h"
#include "VocBase/KeyGenerator.h"
#include "Cluster/ClusterMethods.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
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
  
////////////////////////////////////////////////////////////////////////////////
/// @brief opens the declared collections of the transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::openCollections() {
  if (_trx == nullptr) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  if (_setupState != TRI_ERROR_NO_ERROR) {
    return _setupState;
  }

  if (!_isReal) {
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_EnsureCollectionsTransaction(_trx, _nestingLevel);

  return res;
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
                                 std::vector<TRI_doc_mptr_copy_t>& docs,
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
                     std::vector<TRI_doc_mptr_copy_t>& docs,
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

int Transaction::any(TRI_transaction_collection_t* trxCollection,
                     TRI_doc_mptr_copy_t* mptr) {
  TRI_ASSERT(mptr != nullptr);
  TRI_document_collection_t* document = documentCollection(trxCollection);

  // READ-LOCK START
  int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  if (orderDitch(trxCollection) == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  auto idx = document->primaryIndex();
  arangodb::basics::BucketPosition intPos;
  arangodb::basics::BucketPosition pos;
  uint64_t step = 0;
  uint64_t total = 0;

  TRI_doc_mptr_t* found = idx->lookupRandom(this, intPos, pos, step, total);
  if (found != nullptr) {
    *mptr = *found;
  }
  this->unlock(trxCollection, TRI_TRANSACTION_READ);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read all documents
////////////////////////////////////////////////////////////////////////////////

int Transaction::all(TRI_transaction_collection_t* trxCollection,
                     std::vector<std::string>& ids, bool lock) {
  TRI_document_collection_t* document = documentCollection(trxCollection);

  if (lock) {
    // READ-LOCK START
    int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  if (orderDitch(trxCollection) == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  auto idx = document->primaryIndex();
  size_t used = idx->size();

  if (used > 0) {
    arangodb::basics::BucketPosition step;
    uint64_t total = 0;

    while (true) {
      TRI_doc_mptr_t const* mptr = idx->lookupSequential(this, step, total);

      if (mptr == nullptr) {
        break;
      }
      ids.emplace_back(TRI_EXTRACT_MARKER_KEY(mptr));
    }
  }

  if (lock) {
    this->unlock(trxCollection, TRI_TRANSACTION_READ);
    // READ-LOCK END
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers, using skip and limit
////////////////////////////////////////////////////////////////////////////////

int Transaction::readSlice(TRI_transaction_collection_t* trxCollection,
                           std::vector<TRI_doc_mptr_copy_t>& docs, int64_t skip,
                           uint64_t limit, uint64_t& total) {
  TRI_document_collection_t* document = documentCollection(trxCollection);

  if (limit == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  // READ-LOCK START
  int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (orderDitch(trxCollection) == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  uint64_t count = 0;
  auto idx = document->primaryIndex();
  TRI_doc_mptr_t const* mptr = nullptr;

  if (skip < 0) {
    arangodb::basics::BucketPosition position;
    do {
      mptr = idx->lookupSequentialReverse(this, position);
      ++skip;
    } while (skip < 0 && mptr != nullptr);

    if (mptr == nullptr) {
      this->unlock(trxCollection, TRI_TRANSACTION_READ);
      // To few elements, skipped all
      return TRI_ERROR_NO_ERROR;
    }

    do {
      mptr = idx->lookupSequentialReverse(this, position);

      if (mptr == nullptr) {
        break;
      }
      ++count;
      docs.emplace_back(*mptr);
    } while (count < limit);

    this->unlock(trxCollection, TRI_TRANSACTION_READ);
    return TRI_ERROR_NO_ERROR;
  }
  arangodb::basics::BucketPosition position;

  while (skip > 0) {
    mptr = idx->lookupSequential(this, position, total);
    --skip;
    if (mptr == nullptr) {
      // To few elements, skipped all
      this->unlock(trxCollection, TRI_TRANSACTION_READ);
      return TRI_ERROR_NO_ERROR;
    }
  }

  do {
    mptr = idx->lookupSequential(this, position, total);
    if (mptr == nullptr) {
      break;
    }
    ++count;
    docs.emplace_back(*mptr);
  } while (count < limit);

  this->unlock(trxCollection, TRI_TRANSACTION_READ);

  return TRI_ERROR_NO_ERROR;
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
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief read one or multiple documents in a collection, local
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::documentLocal(std::string const& collectionName,
                                           VPackSlice const& value,
                                           OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionId(collectionName);

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
 
  TRI_doc_mptr_copy_t mptr;
  int res = document->read(this, key, &mptr, !isLocked(document, TRI_TRANSACTION_READ));

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
  
  TRI_ASSERT(mptr.getDataPtr() != nullptr);
  if (expectedRevision != 0 && expectedRevision != mptr._rid) {
    // still return 
    VPackBuilder resultBuilder;
    resultBuilder.openObject();
    resultBuilder.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(std::string(collectionName + "/" + key)));
    resultBuilder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(std::to_string(mptr._rid)));
    resultBuilder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
    resultBuilder.close();

    return OperationResult(resultBuilder.steal(), nullptr, "",
        TRI_ERROR_ARANGO_CONFLICT,
        options.waitForSync || document->_info.waitForSync()); 
  }
  
  VPackBuilder resultBuilder;
  if (!options.silent) {
    resultBuilder.add(VPackSlice(mptr.vpack()));
  }

  return OperationResult(resultBuilder.steal(), StorageOptions::getCustomTypeHandler(_vocbase), "", TRI_ERROR_NO_ERROR, false); 
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

  if (value.isArray()) {
    // multi-document variant is not yet implemented
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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
  std::map<std::string, std::string> headers;
  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  int res = arangodb::createDocumentOnCoordinator(
      _vocbase->_name, collectionName, options.waitForSync,
      value, headers, responseCode, resultHeaders, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
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
  
  TRI_voc_cid_t cid = resolver()->getCollectionId(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  // add missing attributes for document (_id, _rev, _key)
  VPackBuilder merge;
  merge.openObject();
   
  // generate a new tick value
  TRI_voc_tick_t const revisionId = TRI_NewTickServer();
  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));

  auto key = value.get(TRI_VOC_ATTRIBUTE_KEY);

  if (key.isNone()) {
    // "_key" attribute not present in object
    merge.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(document->_keyGenerator->generate(revisionId)));
  } else if (!key.isString()) {
    // "_key" present but wrong type
    return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  } else {
    int res = document->_keyGenerator->validate(key.copyString(), false);

    if (res != TRI_ERROR_NO_ERROR) {
      // invalid key value
      return OperationResult(res);
    }
  }
  
  // add _rev attribute
  merge.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(std::to_string(revisionId)));

  // add _id attribute
  uint8_t* p = merge.add(TRI_VOC_ATTRIBUTE_ID, VPackValuePair(9ULL, VPackValueType::Custom));
  *p++ = 0xf3; // custom type for _id
  MarkerHelper::storeNumber<uint64_t>(p, cid, sizeof(uint64_t));

  merge.close();

  VPackBuilder toInsert = VPackCollection::merge(value, merge.slice(), false, false); 
  VPackSlice insertSlice = toInsert.slice();
  
  if (orderDitch(trxCollection(cid)) == nullptr) {
    return OperationResult(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_doc_mptr_copy_t mptr;
  int res = document->insert(this, &insertSlice, &mptr, options, !isLocked(document, TRI_TRANSACTION_WRITE));
  
  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);
  
  VPackSlice vpack(mptr.vpack());
  std::string resultKey = VPackSlice(mptr.vpack()).get(TRI_VOC_ATTRIBUTE_KEY).copyString(); 

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(std::string(collectionName + "/" + resultKey)));
  resultBuilder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(vpack.get(TRI_VOC_ATTRIBUTE_REV).copyString()));
  resultBuilder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(resultKey));
  resultBuilder.close();

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR,
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
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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
  TRI_voc_cid_t cid = resolver()->getCollectionId(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  // read expected revision
  TRI_voc_rid_t expectedRevision = Transaction::extractRevisionId(&oldValue);

  // generate a new tick value
  TRI_voc_tick_t const revisionId = TRI_NewTickServer();
  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));


  VPackBuilder builder;
  builder.openObject();

  VPackObjectIterator it(newValue);
  while (it.valid()) {
    // let all but the system attributes pass
    std::string key = it.key().copyString();
    if (key[0] != '_' ||
        (key != TRI_VOC_ATTRIBUTE_KEY &&
         key != TRI_VOC_ATTRIBUTE_ID &&
         key != TRI_VOC_ATTRIBUTE_REV &&
         key != TRI_VOC_ATTRIBUTE_FROM &&
         key != TRI_VOC_ATTRIBUTE_TO)) {
      builder.add(it.key().copyString(), it.value());
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
  
  TRI_doc_mptr_copy_t mptr;
  TRI_voc_rid_t actualRevision = 0;
  TRI_doc_update_policy_t policy(expectedRevision == 0 ? TRI_DOC_UPDATE_LAST_WRITE : TRI_DOC_UPDATE_ERROR, expectedRevision, &actualRevision);

  int res = lock(trxCollection(cid), TRI_TRANSACTION_WRITE);
  
  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  res = document->update(this, &oldValue, &sanitized, &mptr, &policy, options, !isLocked(document, TRI_TRANSACTION_WRITE));

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);

  std::string idString(std::to_string(actualRevision));
  
  VPackSlice vpack(mptr.vpack());
  std::string resultKey = VPackSlice(mptr.vpack()).get(TRI_VOC_ATTRIBUTE_KEY).copyString(); 

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(std::string(collectionName + "/" + resultKey)));
  resultBuilder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(vpack.get(TRI_VOC_ATTRIBUTE_REV).copyString()));
  resultBuilder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(resultKey));
  resultBuilder.add("_oldRev", VPackValue(idString));
  resultBuilder.close();

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

  if (oldValue.isArray() || newValue.isArray()) {
    // multi-document variant is not yet implemented
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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
  TRI_voc_cid_t cid = resolver()->getCollectionId(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  
  // read expected revision
  TRI_voc_rid_t expectedRevision = Transaction::extractRevisionId(&oldValue);

  // generate a new tick value
  TRI_voc_tick_t const revisionId = TRI_NewTickServer();
  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));


  VPackBuilder builder;
  builder.openObject();

  VPackObjectIterator it(newValue);
  while (it.valid()) {
    // let all but the system attributes pass
    std::string key = it.key().copyString();
    if (key[0] != '_' ||
        (key != TRI_VOC_ATTRIBUTE_KEY &&
         key != TRI_VOC_ATTRIBUTE_ID &&
         key != TRI_VOC_ATTRIBUTE_REV &&
         key != TRI_VOC_ATTRIBUTE_FROM &&
         key != TRI_VOC_ATTRIBUTE_TO)) {
      builder.add(it.key().copyString(), it.value());
    }
    it.next();
  }
  // finally add the (new) _rev attributes
  builder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(std::to_string(revisionId)));
  builder.close();

  VPackSlice sanitized = builder.slice();

  if (orderDitch(trxCollection(cid)) == nullptr) {
    return OperationResult(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  TRI_doc_mptr_copy_t mptr;
  TRI_voc_rid_t actualRevision = 0;
  TRI_doc_update_policy_t policy(expectedRevision == 0 ? TRI_DOC_UPDATE_LAST_WRITE : TRI_DOC_UPDATE_ERROR, expectedRevision, &actualRevision);

  int res = lock(trxCollection(cid), TRI_TRANSACTION_WRITE);
  
  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  res = document->replace(this, &oldValue, &sanitized, &mptr, &policy, options, !isLocked(document, TRI_TRANSACTION_WRITE));

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);

  std::string idString(std::to_string(actualRevision));
  
  VPackSlice vpack(mptr.vpack());
  std::string resultKey = VPackSlice(mptr.vpack()).get(TRI_VOC_ATTRIBUTE_KEY).copyString(); 

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(std::string(collectionName + "/" + resultKey)));
  resultBuilder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(vpack.get(TRI_VOC_ATTRIBUTE_REV).copyString()));
  resultBuilder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(resultKey));
  resultBuilder.add("_oldRev", VPackValue(idString));
  resultBuilder.close();

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR,
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
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief remove one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::removeLocal(std::string const& collectionName,
                                         VPackSlice const& value,
                                         OperationOptions& options) {
  TRI_voc_cid_t cid = resolver()->getCollectionId(collectionName);

  if (cid == 0) {
    return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  // TODO: clean this up
  TRI_document_collection_t* document = documentCollection(trxCollection(cid));
 
  std::string key; 
  TRI_voc_rid_t expectedRevision = Transaction::extractRevisionId(&value);

  VPackBuilder builder;
  builder.openObject();

  // extract _key
  if (value.isObject()) {
    VPackSlice k = value.get(TRI_VOC_ATTRIBUTE_KEY);
    if (!k.isString()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    }
    builder.add(TRI_VOC_ATTRIBUTE_KEY, k);
  } else if (value.isString()) {
    builder.add(TRI_VOC_ATTRIBUTE_KEY, value);
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

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add(TRI_VOC_ATTRIBUTE_ID, VPackValue(std::string(collectionName + "/" + key)));
  resultBuilder.add(TRI_VOC_ATTRIBUTE_REV, VPackValue(std::to_string(actualRevision)));
  resultBuilder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(key));
  resultBuilder.close();

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR,
                         options.waitForSync); 
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
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all documents in a collection, local
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::truncateLocal(std::string const& collectionName,
                                           OperationOptions& options) {
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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
  TRI_voc_cid_t cid = resolver()->getCollectionId(collectionName);

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

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR, false);
}


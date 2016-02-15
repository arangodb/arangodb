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

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief if this pointer is set to an actual set, then for each request
/// sent to a shardId using the ClusterComm library, an X-Arango-Nolock
/// header is generated.
////////////////////////////////////////////////////////////////////////////////

thread_local std::unordered_set<std::string>* Transaction::_makeNolockHeaders =
    nullptr;
  
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
  int res;

  if (errorNum == TRI_ERROR_NO_ERROR) {
    // there was no previous error, so we'll commit
    res = this->commit();
  } else {
    // there was a previous error, so we'll abort
    this->abort();

    // return original error number
    res = errorNum;
  }

  return res;
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

int Transaction::readRandom(TRI_transaction_collection_t* trxCollection,
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

int Transaction::readAny(TRI_transaction_collection_t* trxCollection,
                         TRI_doc_mptr_copy_t* mptr) {
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

int Transaction::readAll(TRI_transaction_collection_t* trxCollection,
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

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers
////////////////////////////////////////////////////////////////////////////////

int Transaction::readSlice(TRI_transaction_collection_t* trxCollection,
                           std::vector<TRI_doc_mptr_t const*>& docs) {
  TRI_document_collection_t* document = documentCollection(trxCollection);
  // READ-LOCK START
  int res = this->lock(trxCollection, TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (orderDitch(trxCollection) == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  arangodb::basics::BucketPosition position;
  uint64_t total = 0;
  auto idx = document->primaryIndex();
  docs.reserve(idx->size());

  while (true) {
    TRI_doc_mptr_t const* mptr = idx->lookupSequential(this, position, total);

    if (mptr == nullptr) {
      break;
    }

    docs.emplace_back(mptr);
  }

  this->unlock(trxCollection, TRI_TRANSACTION_READ);
  return TRI_ERROR_NO_ERROR;
}

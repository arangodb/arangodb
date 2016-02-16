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

#ifndef ARANGOD_UTILS_SINGLE_COLLECTION_TRANSACTION_H
#define ARANGOD_UTILS_SINGLE_COLLECTION_TRANSACTION_H 1

#include "Basics/Common.h"
#include "Basics/voc-errors.h"
#include "Basics/StringUtils.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionContext.h"
#include "VocBase/Ditch.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class SingleCollectionTransaction : public Transaction {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the transaction, using a collection id
  //////////////////////////////////////////////////////////////////////////////

  SingleCollectionTransaction(TransactionContext* transactionContext,
                              TRI_vocbase_t* vocbase, TRI_voc_cid_t cid,
                              TRI_transaction_type_e accessType)
      : Transaction(transactionContext, vocbase, 0),
        _cid(cid),
        _trxCollection(nullptr),
        _documentCollection(nullptr),
        _accessType(accessType) {
    // add the (sole) collection
    this->addCollection(cid, _accessType);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the transaction, using a collection name
  //////////////////////////////////////////////////////////////////////////////

  SingleCollectionTransaction(TransactionContext* transactionContext,
                              TRI_vocbase_t* vocbase, std::string const& name,
                              TRI_transaction_type_e accessType)
      : Transaction(transactionContext, vocbase, 0),
        _cid(0),
        _trxCollection(nullptr),
        _documentCollection(nullptr),
        _accessType(accessType) {
    // add the (sole) collection
    if (setupState() == TRI_ERROR_NO_ERROR) {
      _cid = this->resolver()->getCollectionId(name);

      this->addCollection(_cid, _accessType);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief end the transaction
  //////////////////////////////////////////////////////////////////////////////

  ~SingleCollectionTransaction() {}

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the underlying transaction collection
  //////////////////////////////////////////////////////////////////////////////

  inline TRI_transaction_collection_t* trxCollection() {
    TRI_ASSERT(this->_cid > 0);

    if (this->_trxCollection == nullptr) {
      this->_trxCollection =
          TRI_GetCollectionTransaction(this->_trx, this->_cid, _accessType);

      if (this->_trxCollection != nullptr &&
          this->_trxCollection->_collection != nullptr) {
        this->_documentCollection =
            this->_trxCollection->_collection->_collection;
      }
    }

    TRI_ASSERT(this->_trxCollection != nullptr);
    return this->_trxCollection;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the underlying document collection
  /// note that we have two identical versions because this is called
  /// in two different situations
  //////////////////////////////////////////////////////////////////////////////

  inline TRI_document_collection_t* documentCollection() {
    if (this->_documentCollection != nullptr) {
      return this->_documentCollection;
    }

    this->trxCollection();
    TRI_ASSERT(this->_documentCollection != nullptr);

    return this->_documentCollection;
  }

  inline TRI_document_collection_t* documentCollection(TRI_voc_cid_t) {
    return documentCollection();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the ditch for the collection
  /// note that the ditch must already exist
  /// furthermore note that we have two calling conventions because this
  /// is called in two different ways
  //////////////////////////////////////////////////////////////////////////////

  inline arangodb::DocumentDitch* ditch() {
    TRI_transaction_collection_t* trxCollection = this->trxCollection();
    TRI_ASSERT(trxCollection->_ditch != nullptr);

    return trxCollection->_ditch;
  }

  inline arangodb::DocumentDitch* ditch(TRI_voc_cid_t) { return ditch(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not a ditch is available for a collection
  //////////////////////////////////////////////////////////////////////////////

  inline bool hasDitch() {
    TRI_transaction_collection_t* trxCollection = this->trxCollection();

    return (trxCollection->_ditch != nullptr);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the underlying collection's id
  //////////////////////////////////////////////////////////////////////////////

  inline TRI_voc_cid_t cid() const { return _cid; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief explicitly lock the underlying collection for read access
  //////////////////////////////////////////////////////////////////////////////

  int lockRead() {
    return this->lock(this->trxCollection(), TRI_TRANSACTION_READ);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief explicitly unlock the underlying collection after read access
  //////////////////////////////////////////////////////////////////////////////

  int unlockRead() {
    return this->unlock(this->trxCollection(), TRI_TRANSACTION_READ);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief explicitly lock the underlying collection for write access
  //////////////////////////////////////////////////////////////////////////////

  int lockWrite() {
    return this->lock(this->trxCollection(), TRI_TRANSACTION_WRITE);
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief collection id
  //////////////////////////////////////////////////////////////////////////////

  TRI_voc_cid_t _cid;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief trxCollection cache
  //////////////////////////////////////////////////////////////////////////////

  TRI_transaction_collection_t* _trxCollection;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief TRI_document_collection_t* cache
  //////////////////////////////////////////////////////////////////////////////

  TRI_document_collection_t* _documentCollection;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief collection access type
  //////////////////////////////////////////////////////////////////////////////

  TRI_transaction_type_e _accessType;
};
}

#endif

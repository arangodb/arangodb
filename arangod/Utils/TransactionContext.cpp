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

#include "TransactionContext.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/Ditch.h"
#include "VocBase/document-collection.h"
#include "Wal/LogfileManager.h"

#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

// custom type value handler, used for deciphering the _id attribute
struct CustomTypeHandler : public VPackCustomTypeHandler {
  CustomTypeHandler(TRI_vocbase_t* vocbase, CollectionNameResolver const* resolver)
      : vocbase(vocbase), resolver(resolver) {}

  ~CustomTypeHandler() {} 

  void dump(VPackSlice const& value, VPackDumper* dumper,
            VPackSlice const& base) override final {
    if (value.head() != 0xf3) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid custom type");
    }

    // _id
    if (!base.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid value type");
    }
  
    uint64_t cid = DatafileHelper::ReadNumber<uint64_t>(value.begin() + 1, sizeof(uint64_t));
    char buffer[512];  // This is enough for collection name + _key
    size_t len = resolver->getCollectionName(&buffer[0], cid);
    buffer[len] = '/';
    VPackSlice key = base.get(TRI_VOC_ATTRIBUTE_KEY);

    VPackValueLength keyLength;
    char const* p = key.getString(keyLength);
    if (p == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid _key value");
    }
    memcpy(&buffer[len + 1], p, keyLength);
    dumper->appendString(&buffer[0], len + 1 + keyLength);
  }
  
  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) override final {
    if (value.head() != 0xf3) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid custom type");
    }

    // _id
    if (!base.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid value type");
    }
    
    uint64_t cid = DatafileHelper::ReadNumber<uint64_t>(value.begin() + 1, sizeof(uint64_t));
    std::string result(resolver->getCollectionName(cid));
    result.push_back('/');
    VPackSlice key = base.get(TRI_VOC_ATTRIBUTE_KEY);

    VPackValueLength keyLength;
    char const* p = key.getString(keyLength);
    if (p == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid _key value");
    }
    result.append(p, keyLength);
    return result;
  }

  TRI_vocbase_t* vocbase;
  CollectionNameResolver const* resolver;
};

//////////////////////////////////////////////////////////////////////////////
/// @brief create the context
//////////////////////////////////////////////////////////////////////////////

TransactionContext::TransactionContext(TRI_vocbase_t* vocbase) 
    : _vocbase(vocbase), 
      _resolver(nullptr), 
      _customTypeHandler(),
      _transaction{ 0, false }, 
      _ownsResolver(false) {}

//////////////////////////////////////////////////////////////////////////////
/// @brief destroy the context
//////////////////////////////////////////////////////////////////////////////

TransactionContext::~TransactionContext() {
  // unregister the transaction from the logfile manager
  if (_transaction.id > 0) {
    arangodb::wal::LogfileManager::instance()->unregisterTransaction(_transaction.id, _transaction.hasFailedOperations);
  }

  for (auto& it : _ditches) {
    // we're done with this ditch
    auto& ditch = it.second;
    ditch->ditches()->freeDocumentDitch(ditch, true /* fromTransaction */);
    // If some external entity is still using the ditch, it is kept!
  }

  if (_ownsResolver) {
    delete _resolver;
    _resolver = nullptr;
  }
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief order a document ditch for the collection
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackCustomTypeHandler> TransactionContext::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    _customTypeHandler.reset(new CustomTypeHandler(_vocbase, getResolver()));
  }

  TRI_ASSERT(_customTypeHandler != nullptr);
  return _customTypeHandler;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief order a document ditch for the collection
//////////////////////////////////////////////////////////////////////////////

DocumentDitch* TransactionContext::orderDitch(TRI_document_collection_t* document) {
  TRI_voc_cid_t cid = document->_info.id();

  auto it = _ditches.find(cid);

  if (it != _ditches.end()) {
    // tell everyone else this ditch is still in use,
    // at least until the transaction is over
    (*it).second->setUsedByTransaction();
    // ditch already exists, return it
    return (*it).second;
  }

  // this method will not throw, but may return a nullptr
  auto ditch = document->ditches()->createDocumentDitch(true, __FILE__, __LINE__);

  if (ditch != nullptr) {
    try {
      _ditches.emplace(cid, ditch);
    }
    catch (...) {
      ditch->ditches()->freeDocumentDitch(ditch, true);
      ditch = nullptr; // return a nullptr
    }
  }

  return ditch;
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief return the ditch for a collection
/// this will return a nullptr if no ditch exists
//////////////////////////////////////////////////////////////////////////////
  
DocumentDitch* TransactionContext::ditch(TRI_voc_cid_t cid) const {
  auto it = _ditches.find(cid);

  if (it == _ditches.end()) {
    return nullptr;
  }
  return (*it).second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a resolver
////////////////////////////////////////////////////////////////////////////////
    
void TransactionContext::createResolver() {
  TRI_ASSERT(_resolver == nullptr);
  _resolver = new CollectionNameResolver(_vocbase);
  _ownsResolver = true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief unregister the transaction
/// this will save the transaction's id and status locally
//////////////////////////////////////////////////////////////////////////////

void TransactionContext::storeTransactionResult(TRI_voc_tid_t id, bool hasFailedOperations) {
  TRI_ASSERT(_transaction.id == 0);

  _transaction.id = id;
  _transaction.hasFailedOperations = hasFailedOperations;
}


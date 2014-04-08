////////////////////////////////////////////////////////////////////////////////
/// @brief transaction
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Transaction.h"
#include "Transaction/Manager.h"
#include "VocBase/vocbase.h"

using namespace triagens::transaction;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction
////////////////////////////////////////////////////////////////////////////////

Transaction::Transaction (Manager* manager,
                          IdType id,
                          triagens::arango::CollectionNameResolver const& resolver,
                          TRI_vocbase_t* vocbase,
                          bool singleOperation,
                          bool waitForSync) 
  : _manager(manager),
    _id(id),
    _state(StateType::STATE_UNINITIALISED),
    _resolver(resolver),
    _vocbase(vocbase),
    _singleOperation(singleOperation),
    _waitForSync(waitForSync),
    _collections(),
    _startTime() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction
////////////////////////////////////////////////////////////////////////////////

Transaction::~Transaction () {
  if (state() != StateType::STATE_COMMITTED && 
      state() != StateType::STATE_ABORTED) {
    this->abort();
  }

  for (auto it = _collections.begin(); it != _collections.end(); ++it) {
    Collection* collection = (*it).second;

    delete collection;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the transaction
////////////////////////////////////////////////////////////////////////////////
 
int Transaction::addCollection (string const& name,
                                Collection::AccessType accessType) {
  TRI_voc_cid_t id = _resolver.getCollectionId(name);

  return addCollection(id, accessType);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the transaction
////////////////////////////////////////////////////////////////////////////////
 
int Transaction::addCollection (TRI_voc_cid_t id,
                                Collection::AccessType accessType) {
  if (id == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  if (state() != StateType::STATE_BEGUN) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  // add the collection to the list
  auto it = _collections.find(id);

  if (it != _collections.end()) {
    // collection was already added
    if (accessType == Collection::AccessType::WRITE && 
        ! (*it).second->allowWriteAccess()) {
      return TRI_ERROR_TRANSACTION_INTERNAL;
    }

    // ok
    return TRI_ERROR_NO_ERROR;
  }

  // register the collection
  _collections.insert(make_pair(id, new Collection(id, accessType)));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::begin () {
  return _manager->beginTransaction(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////
        
int Transaction::commit () {
  return _manager->commitTransaction(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////
        
int Transaction::abort () {
  return _manager->abortTransaction(this);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

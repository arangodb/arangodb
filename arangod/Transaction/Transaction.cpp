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

#include "BasicsC/logging.h"
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
                          TRI_vocbase_t* vocbase,
                          bool singleOperation,
                          bool waitForSync) 
  : _manager(manager),
    _id(id),
    _state(StateType::STATE_UNINITIALISED),
    _resolver(vocbase),
    _vocbase(vocbase),
    _singleOperation(singleOperation),
    _waitForSync(waitForSync),
    _collections(),
    _startTime() {

  LOG_INFO("creating transaction %llu", (unsigned long long) id);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction
////////////////////////////////////////////////////////////////////////////////

Transaction::~Transaction () {
  if (state() != StateType::STATE_COMMITTED && 
      state() != StateType::STATE_ABORTED) {
    this->rollback();
  }

  for (auto it = _collections.begin(); it != _collections.end(); ++it) {
    Collection* collection = (*it).second;

    delete collection;
  }
  
  LOG_INFO("destroyed transaction %llu", (unsigned long long) _id);
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

  auto s = state();

  if (accessType == Collection::AccessType::WRITE &&
      s != StateType::STATE_UNINITIALISED) {
    // cannot add write access to an already running collection
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }
  
  if (s != StateType::STATE_UNINITIALISED &&
      s != StateType::STATE_BEGUN) {
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
  
  LOG_INFO("adding collection %llu to transaction %llu", (unsigned long long) id, (unsigned long long) _id);

  // register the collection
  _collections.insert(make_pair(id, new Collection(id, accessType)));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::begin () {
  LOG_INFO("beginning transaction %llu", (unsigned long long) _id);
  return _manager->beginTransaction(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////
        
int Transaction::commit (bool waitForSync) {
  LOG_INFO("committing transaction %llu", (unsigned long long) _id);
  return _manager->commitTransaction(this, waitForSync || _waitForSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rollback a transaction
////////////////////////////////////////////////////////////////////////////////
        
int Transaction::rollback () {
  LOG_INFO("rolling back transaction %llu", (unsigned long long) _id);
  return _manager->rollbackTransaction(this);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

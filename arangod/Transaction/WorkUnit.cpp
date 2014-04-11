////////////////////////////////////////////////////////////////////////////////
/// @brief transaction context
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

#include "WorkUnit.h"
#include "BasicsC/logging.h"
#include "Transaction/Collection.h"
#include "Transaction/Context.h"
#include "Transaction/Helper.h"
#include "Transaction/Marker.h"
#include "Transaction/Transaction.h"
#include "Utils/Exception.h"
#include "VocBase/vocbase.h"
#include "Wal/LogfileManager.h"

#define WORKUNIT_LOG(msg) LOG_INFO("workunit #%llu: %s", (unsigned long long) _id, msg)

using namespace std;
using namespace triagens::transaction;
using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction work unit
////////////////////////////////////////////////////////////////////////////////

WorkUnit::WorkUnit (Context* context, 
                    bool singleOperation)
  : State(),
    _context(context),
    _id(context->nextWorkUnitId()),
    _level(context->level()),
    _singleOperation(singleOperation),
    _done(false) {

  _context->increaseRefCount();
  
  _context->startWorkUnit(this);
  WORKUNIT_LOG("starting");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a transaction work unit
////////////////////////////////////////////////////////////////////////////////

WorkUnit::~WorkUnit () {
  WORKUNIT_LOG("destroyed");
  done();
  
  _context->decreaseRefCount();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the unit, assert a specific collection type
////////////////////////////////////////////////////////////////////////////////

Collection* WorkUnit::addCollection (string const& name,
                                     Collection::AccessType accessType,
                                     TRI_col_type_e collectionType,
                                     bool lockResponsibility,
                                     bool locked) {
  TRI_vocbase_col_t* collection = _context->resolveCollection(name);
 
  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_STRING(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, name);
  }

  if (collection->_type != static_cast<TRI_col_type_t>(collectionType)) {
    THROW_ARANGO_EXCEPTION_STRING(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID, name);
  }
   
  return addCollection(collection->_cid, collection, accessType, lockResponsibility, locked);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the unit
////////////////////////////////////////////////////////////////////////////////

Collection* WorkUnit::addCollection (string const& name,
                                     Collection::AccessType accessType,
                                     bool lockResponsibility,
                                     bool locked) {
  TRI_vocbase_col_t* collection = _context->resolveCollection(name);
  
  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_STRING(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, name);
  }
   
  return addCollection(collection->_cid, collection, accessType, lockResponsibility, locked);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the unit
////////////////////////////////////////////////////////////////////////////////
        
Collection* WorkUnit::addCollection (TRI_voc_cid_t id,
                                     TRI_vocbase_col_t* collection,
                                     Collection::AccessType accessType,
                                     bool lockResponsibility,
                                     bool locked) {
  if (id == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  auto it = _collections.find(id);

  if (it != _collections.end()) {
    if (accessType == Collection::AccessType::WRITE &&
        ! (*it).second->allowWriteAccess()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_INTERNAL);
    }

    return (*it).second;
  }

  Collection* c = _context->findCollection(id);

  if (c == nullptr) {
    // no previous collection found, now insert it
    c = new Collection(collection, accessType, lockResponsibility, locked);

    _collections.insert(make_pair(id, c));
  }

  return c;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a collection in a unit of work
////////////////////////////////////////////////////////////////////////////////

Collection* WorkUnit::findCollection (TRI_voc_cid_t id) const {
  auto it = _collections.find(id);
  if (it != _collections.end()) {
    return (*it).second;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a unit of work
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::begin () {
  if (state() != State::StateType::UNINITIALISED) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }
 
  WORKUNIT_LOG("begin"); 
  
  int res = TRI_ERROR_NO_ERROR;  
  for (auto& it : _collections) {
    res = it.second->use();
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
    
    res = it.second->lock();
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }
 
  if (! isTopLevel()) {
    res = TRI_ERROR_NO_ERROR;
  }
  else {
    Transaction* transaction = _context->transaction();
    res = transaction->begin();
  }
  
  setState(State::StateType::BEGUN);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a unit of work
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::commit (bool waitForSync) {
  if (state() != State::StateType::BEGUN) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }
  
  WORKUNIT_LOG("commit"); 
   
  int res;
  if (! isTopLevel()) {
    res = TRI_ERROR_NO_ERROR;
  }
  else {
    Transaction* transaction = _context->transaction();
    res = transaction->commit(waitForSync);
  }
  
  done();
  setState(State::StateType::COMMITTED);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rollback a unit of work
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::rollback () {
  if (state() != State::StateType::BEGUN) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }
  
  WORKUNIT_LOG("rollback"); 

  int res;
  if (! isTopLevel()) {
    res = TRI_ERROR_NO_ERROR;
  }
  else {
    Transaction* transaction = _context->transaction();
    res = transaction->rollback();
  }
  
  done();
  setState(State::StateType::ABORTED);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a single document
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::saveDocument (Collection* collection,
                            triagens::basics::Bson& document,
                            bool waitForSync) {
  // generate a tick value
  TRI_voc_tick_t revision = collection->generateRevision();

  // validate or create key
  string key(Helper::documentKey(document));
  if (key.empty()) {
    key = collection->generateKey(revision);
    Helper::appendKey(document, key); 
  }
  else {
    collection->validateKey(key);
  }

  DocumentMarker marker(collection->databaseId(), collection->id(), key, revision, document);

  LogfileManager* logfileManager = _context->logfileManager();
       
  int res = logfileManager->allocateAndWrite(marker.buffer, 
                                             marker.size, 
                                             waitForSync);

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
  
////////////////////////////////////////////////////////////////////////////////
/// @brief set the unit of work to done
////////////////////////////////////////////////////////////////////////////////

void WorkUnit::done () {
  if (! _done) {
    for (auto& it : _collections) {
      it.second->done();
    }

    _context->endWorkUnit(this);
    _done = true;
  }

  cleanup();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup the unit of work
////////////////////////////////////////////////////////////////////////////////

void WorkUnit::cleanup () {
  for (auto it = _collections.begin(); it != _collections.end(); ++it) {
    Collection* collection = (*it).second;
    delete collection;
  }
  
  _collections.clear();
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

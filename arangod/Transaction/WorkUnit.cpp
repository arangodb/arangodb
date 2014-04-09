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
#include "Transaction/Collection.h"
#include "Transaction/Context.h"
#include "Transaction/Transaction.h"
#include "VocBase/vocbase.h"

using namespace triagens::transaction;

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a transaction work unit
////////////////////////////////////////////////////////////////////////////////

WorkUnit::~WorkUnit () {
  done();
  _context->decreaseRefCount();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the unit
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::addCollection (string const& name,
                             Collection::AccessType accessType,
                             bool lockResponsibility,
                             bool locked) {
  TRI_vocbase_col_t* collection = _context->resolveCollection(name);
  
  if (collection == nullptr) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
   
  return addCollection(collection->_cid, collection, accessType, lockResponsibility, locked);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the unit
////////////////////////////////////////////////////////////////////////////////
        
int WorkUnit::addCollection (TRI_voc_cid_t id,
                             TRI_vocbase_col_t* collection,
                             Collection::AccessType accessType,
                             bool lockResponsibility,
                             bool locked) {
  if (id == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  auto it = _collections.find(id);

  if (it != _collections.end()) {
    Collection* collection = (*it).second;
    if (accessType == Collection::AccessType::WRITE &&
        ! collection->allowWriteAccess()) {
      return TRI_ERROR_TRANSACTION_INTERNAL;
    }

    return TRI_ERROR_NO_ERROR;
  }

  Collection* previous = _context->findCollection(id);

  if (previous != nullptr) {
    // collection already present in a top-level work unit
    return TRI_ERROR_NO_ERROR;
  }

  _collections.insert(make_pair(id, new Collection(id, collection, accessType, lockResponsibility, locked)));

  return TRI_ERROR_NO_ERROR;
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
 
  int res;
  if (! isTopLevel()) {
    // TODO: implement begin
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
   
  int res;
  if (! isTopLevel()) {
    // TODO: implement commit
    res = TRI_ERROR_NO_ERROR;
  }
  else {
    Transaction* transaction = _context->transaction();
    res = transaction->commit(waitForSync);
  }
  
  setState(State::StateType::COMMITTED);
  done();

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rollback a unit of work
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::rollback () {
  if (state() != State::StateType::BEGUN) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  int res;
  if (! isTopLevel()) {
    // TODO: implement rollback
    res = TRI_ERROR_NO_ERROR;
  }
  else {
    Transaction* transaction = _context->transaction();
    res = transaction->rollback();
  }
  
  setState(State::StateType::ABORTED);
  done();

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

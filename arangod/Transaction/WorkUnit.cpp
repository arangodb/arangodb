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
#include "Transaction/Context.h"
#include "Transaction/Transaction.h"

using namespace triagens::transaction;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction work unit
////////////////////////////////////////////////////////////////////////////////

WorkUnit::WorkUnit (Context* context, 
                    TRI_vocbase_t* vocbase,
                    bool singleOperation)
  : _context(context),
    _active(true) {

  assert(context != nullptr);

  _context->increaseRefCount();

  _level = _context->level();
  _context->startWorkUnit(vocbase, singleOperation);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a transaction work unit
////////////////////////////////////////////////////////////////////////////////

WorkUnit::~WorkUnit () {
  deactivate();

  _context->decreaseRefCount();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the unit
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::addCollection (string const& name,
                             Collection::AccessType accessType) {
  Transaction* transaction = _context->transaction();

  return transaction->addCollection(name, accessType);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the unit
////////////////////////////////////////////////////////////////////////////////
        
int WorkUnit::addCollection (TRI_voc_cid_t id,
                             Collection::AccessType accessType) {
  Transaction* transaction = _context->transaction();

  return transaction->addCollection(id, accessType);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a unit of work
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::begin () {
  if (! active()) {
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

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a unit of work
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::commit (bool waitForSync) {
  if (! active()) {
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

  deactivate();
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rollback a unit of work
////////////////////////////////////////////////////////////////////////////////

int WorkUnit::rollback () {
  if (! active()) {
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

  deactivate();
  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief deactive the unit of work
////////////////////////////////////////////////////////////////////////////////

void WorkUnit::deactivate () {
  if (active()) {
    _context->endWorkUnit();
    _active = false;
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

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

#include "Context.h"

#include "BasicsC/logging.h"
#include "Transaction/Manager.h"
#include "Transaction/Transaction.h"

using namespace triagens::transaction;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction context
////////////////////////////////////////////////////////////////////////////////

Context::Context (Manager* manager,
                  Context** globalContext) 
  : _manager(manager),
    _globalContext(globalContext),
    _transaction(0),
    _level(0) {

  LOG_INFO("creating context");
  if (_globalContext != nullptr) {
    *_globalContext = this;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction context
////////////////////////////////////////////////////////////////////////////////

Context::~Context () {
  assert(_level == 0);
  assert(_transaction == nullptr);

  if (_globalContext != nullptr) {
    *_globalContext = nullptr;
  }
  LOG_INFO("destroyed context");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief reuse or create a transaction context
////////////////////////////////////////////////////////////////////////////////
 
Context* Context::getContext (Manager* manager,
                              Context** globalContext) {
  return createContext(manager, globalContext);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction context
////////////////////////////////////////////////////////////////////////////////
 
Context* Context::getContext (Manager* manager) {
  return createContext(manager, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the reference count
////////////////////////////////////////////////////////////////////////////////

void Context::increaseRefCount () {
  ++_refCount;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the reference count
/// the last user that calls this will destroy the context!
////////////////////////////////////////////////////////////////////////////////

void Context::decreaseRefCount () {
  assert(_refCount > 0);

  if (--_refCount == 0) {
    // last user has detached
    delete this;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a new unit of work
////////////////////////////////////////////////////////////////////////////////

int Context::startWorkUnit (TRI_vocbase_t* vocbase,
                            bool singleOperation) {

  if (_level == 0) {
    LOG_INFO("starting top-level work unit");
    assert(_transaction == nullptr);

    _transaction = _manager->createTransaction(vocbase, singleOperation);
    assert(_transaction != nullptr);
  }
  else {
    LOG_INFO("starting nested (%d) work unit", _level);
  }

  ++_level;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end a unit of work
////////////////////////////////////////////////////////////////////////////////

int Context::endWorkUnit () {
  assert(_level > 0);

  --_level;
  
  if (_level == 0) {
    LOG_INFO("ending top-level work unit");

    // final level
    if (_transaction != nullptr) {
      delete _transaction;
      _transaction = nullptr;
    }
  }
  else {
    LOG_INFO("ending nested (%d) work unit", _level);
  }
  
  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new transaction context
////////////////////////////////////////////////////////////////////////////////
 
Context* Context::createContext (Manager* manager,
                                 Context** globalContext) {
  if (globalContext == nullptr ||
      *globalContext == nullptr) {
    return new Context(manager, globalContext);
  }

  return *globalContext;
}


// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

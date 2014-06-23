////////////////////////////////////////////////////////////////////////////////
/// @brief transaction context
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Context.h"

#include "BasicsC/logging.h"
#include "Transaction/Collection.h"
#include "Transaction/Manager.h"
#include "Transaction/Transaction.h"
#include "Transaction/WorkUnit.h"
#include "VocBase/vocbase.h"
#include "Wal/LogfileManager.h"

#define CONTEXT_LOG(msg) LOG_INFO("%s", msg)

using namespace std;
using namespace triagens::transaction;
using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction context
////////////////////////////////////////////////////////////////////////////////

Context::Context (Manager* manager,
                  wal::LogfileManager* logfileManager,
                  TRI_vocbase_t* vocbase,
                  Context** globalContext)
  : _manager(manager),
    _logfileManager(logfileManager),
    _resolver(vocbase),
    _globalContext(globalContext),
    _transaction(0),
    _workUnits(),
    _nextWorkUnitId(0),
    _refCount(0) {

  CONTEXT_LOG("creating context");

  if (_globalContext != nullptr) {
    *_globalContext = this;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction context
////////////////////////////////////////////////////////////////////////////////

Context::~Context () {
  TRI_ASSERT(_workUnits.empty());
  TRI_ASSERT(_transaction == nullptr);

  if (_globalContext != nullptr) {
    *_globalContext = nullptr;
  }

  CONTEXT_LOG("destroyed context");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief reuse or create a transaction context
////////////////////////////////////////////////////////////////////////////////

Context* Context::getContext (Manager* manager,
                              wal::LogfileManager* logfileManager,
                              TRI_vocbase_t* vocbase,
                              Context** globalContext) {
  return createContext(manager, logfileManager, vocbase, globalContext);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction context
////////////////////////////////////////////////////////////////////////////////

Context* Context::getContext (Manager* manager,
                              wal::LogfileManager* logfileManager,
                              TRI_vocbase_t* vocbase) {
  return createContext(manager, logfileManager, vocbase, nullptr);
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
  TRI_ASSERT(_refCount > 0);

  if (--_refCount == 0) {
    // last user has detached
    delete this;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resolve a collection name
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* Context::resolveCollection (std::string const& name) const {
  return const_cast<TRI_vocbase_col_t*>(_resolver.getCollectionStruct(name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a collection in the top-level work units
////////////////////////////////////////////////////////////////////////////////

Collection* Context::findCollection (TRI_voc_cid_t id) const {
  for (auto it = _workUnits.rbegin(); it != _workUnits.rend(); ++it) {
    WorkUnit* workUnit = (*it);

    Collection* collection = workUnit->findCollection(id);

    if (collection != nullptr) {
      // found the collection
      return collection;
    }
  }

  // did not find the collection
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a new unit of work
////////////////////////////////////////////////////////////////////////////////

int Context::startWorkUnit (WorkUnit* workUnit) {
  if (_workUnits.empty()) {
    TRI_ASSERT(_transaction == nullptr);

    _transaction = _manager->createTransaction(workUnit->isSingleOperation());
    TRI_ASSERT(_transaction != nullptr);
  }

  TRI_ASSERT(_transaction != nullptr);

  _workUnits.push_back(workUnit);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end a unit of work
////////////////////////////////////////////////////////////////////////////////

int Context::endWorkUnit (WorkUnit* workUnit) {
  TRI_ASSERT(! _workUnits.empty());

  TRI_ASSERT(_workUnits.back() == workUnit);

  // pop last work unit from stack
  _workUnits.pop_back();

  if (_workUnits.empty()) {
    // final level
    if (_transaction != nullptr) {
      delete _transaction;
      _transaction = nullptr;
    }
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
                                 wal::LogfileManager* logfileManager,
                                 TRI_vocbase_t* vocbase,
                                 Context** globalContext) {
  if (globalContext == nullptr ||
      *globalContext == nullptr) {
    return new Context(manager, logfileManager, vocbase, globalContext);
  }

  return *globalContext;
}


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

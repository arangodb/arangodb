////////////////////////////////////////////////////////////////////////////////
/// @brief transaction manager
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

#include "Manager.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Transaction/IdGenerator.h"
#include "VocBase/vocbase.h"

using namespace triagens::transaction;

////////////////////////////////////////////////////////////////////////////////
/// @brief the transaction manager singleton
////////////////////////////////////////////////////////////////////////////////

static Manager* ManagerInstance = 0;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction manager
////////////////////////////////////////////////////////////////////////////////

Manager::Manager () 
  : _generator(),
    _lock(),
    _transactions() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction manager
////////////////////////////////////////////////////////////////////////////////

Manager::~Manager () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction manager instance
////////////////////////////////////////////////////////////////////////////////

Manager* Manager::instance () {
  assert(ManagerInstance != 0);
  return ManagerInstance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the transaction manager instance
////////////////////////////////////////////////////////////////////////////////

void Manager::initialise () {
  assert(ManagerInstance == 0);

  ManagerInstance = new Manager();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the transaction manager instance
////////////////////////////////////////////////////////////////////////////////

void Manager::shutdown () {
  if (ManagerInstance != 0) {
    delete ManagerInstance;
    ManagerInstance = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction object
////////////////////////////////////////////////////////////////////////////////

Transaction* Manager::createTransaction (TRI_vocbase_t* vocbase) {
  Transaction::IdType id = _generator.next();

  Transaction* transaction = new Transaction(this, id, vocbase);

  return transaction;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a transaction
////////////////////////////////////////////////////////////////////////////////

int Manager::beginTransaction (Transaction* transaction) {
  Transaction::IdType id = transaction->id();

  WRITE_LOCKER(_lock);
  auto it = _transactions.insert(make_pair(id, Transaction::STATE_BEGUN));

  if (it.first == _transactions.end()) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

int Manager::commitTransaction (Transaction* transaction) {
  Transaction::IdType id = transaction->id();
  
  WRITE_LOCKER(_lock);
  auto it = _transactions.find(id);

  if (it == _transactions.end()) {
    return TRI_ERROR_INTERNAL;
  }

  _transactions.erase(id);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////

int Manager::abortTransaction (Transaction* transaction) {
  Transaction::IdType id = transaction->id();
  
  WRITE_LOCKER(_lock);
  auto it = _transactions.find(id);

  if (it == _transactions.end()) {
    return TRI_ERROR_INTERNAL;
  }

  (*it).second = Transaction::STATE_ABORTED;
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the status of a transaction
////////////////////////////////////////////////////////////////////////////////

Transaction::StateType Manager::statusTransaction (Transaction::IdType id) {
  READ_LOCKER(_lock);
  
  auto it = _transactions.find(id);
  if (it == _transactions.end()) {
    return Transaction::STATE_COMMITTED;
  }

  return (*it).second;
}

// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

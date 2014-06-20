////////////////////////////////////////////////////////////////////////////////
/// @brief transaction manager
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

#include "Manager.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Transaction/IdGenerator.h"
#include "Transaction/State.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::transaction;

////////////////////////////////////////////////////////////////////////////////
/// @brief the transaction manager singleton
////////////////////////////////////////////////////////////////////////////////

static Manager* Instance = nullptr;

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
  WRITE_LOCKER(_lock);

  for (auto it = _transactions.begin(); it != _transactions.end(); ++it) {
    Transaction* transaction = (*it).second;

    if (transaction->state() == State::StateType::BEGUN) {
      transaction->setState(State::StateType::ABORTED);
    }
  }

  _transactions.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction manager instance
////////////////////////////////////////////////////////////////////////////////

Manager* Manager::instance () {
  TRI_ASSERT(Instance != nullptr);
  return Instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the transaction manager instance
////////////////////////////////////////////////////////////////////////////////

void Manager::initialise () {
  TRI_ASSERT(Instance == nullptr);

  Instance = new Manager();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the transaction manager instance
////////////////////////////////////////////////////////////////////////////////

void Manager::shutdown () {
  if (Instance != nullptr) {
    delete Instance;
    Instance = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction object
////////////////////////////////////////////////////////////////////////////////

Transaction* Manager::createTransaction (bool singleOperation) {
  TRI_voc_tid_t id = _generator.next();

  Transaction* transaction = new Transaction(this, id, singleOperation);

  WRITE_LOCKER(_lock);
  auto it = _transactions.insert(make_pair(id, transaction));

  if (it.first == _transactions.end()) {
    // couldn't insert transaction
    delete transaction;

    return nullptr;
  }

  return transaction;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the status of a transaction
////////////////////////////////////////////////////////////////////////////////

Transaction::StateType Manager::statusTransaction (TRI_voc_tid_t id) {
  READ_LOCKER(_lock);

  auto it = _transactions.find(id);
  if (it != _transactions.end()) {
    return (*it).second->state();
  }

  // unknown transaction. probably already committed
  return State::StateType::COMMITTED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get oldest still running transaction
////////////////////////////////////////////////////////////////////////////////

TransactionInfo Manager::getOldestRunning () {
  READ_LOCKER(_lock);

  for (auto it = _transactions.begin(); it != _transactions.end(); ++it) {
    Transaction* transaction = (*it).second;

    if (transaction->state() == State::StateType::BEGUN) {
      return TransactionInfo(transaction->id(), transaction->elapsedTime());
    }
  }

  return TransactionInfo();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether any of the specified transactions is still running
////////////////////////////////////////////////////////////////////////////////

bool Manager::containsRunning (vector<TRI_voc_tid_t> const& ids) {
  READ_LOCKER(_lock);

  for (auto it = ids.begin(); it != ids.end(); ++it) {
    auto it2 = _transactions.find(*it);

    if (it2 != _transactions.end()) {
      Transaction* transaction = (*it2).second;

      if (transaction->state() == State::StateType::BEGUN) {
        return true;
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove failed transactions from the failed list
////////////////////////////////////////////////////////////////////////////////

int Manager::removeFailed (vector<TRI_voc_tid_t> const& ids) {
  WRITE_LOCKER(_lock);

  for (auto it = ids.begin(); it != ids.end(); ++it) {
    _transactions.erase(*it);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a transaction
////////////////////////////////////////////////////////////////////////////////

int Manager::beginTransaction (Transaction* transaction) {
  WRITE_LOCKER(_lock);

  // check the transaction state first
  if (transaction->state() != State::StateType::UNINITIALISED) {
    transaction->setState(State::StateType::ABORTED);
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  // sets status and start time stamp
  transaction->setState(State::StateType::BEGUN);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

int Manager::commitTransaction (Transaction* transaction,
                                bool waitForSync) {
  TRI_voc_tid_t id = transaction->id();

  WRITE_LOCKER(_lock);

  if (transaction->state() != State::StateType::BEGUN) {
    // set it to aborted
    transaction->setState(State::StateType::ABORTED);
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  transaction->setState(State::StateType::COMMITTED);

  auto it = _transactions.find(id);

  if (it == _transactions.end()) {
    // not found
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  // erase it in the list of transactions
  _transactions.erase(id);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rollback a transaction
////////////////////////////////////////////////////////////////////////////////

int Manager::rollbackTransaction (Transaction* transaction) {
  WRITE_LOCKER(_lock);

  if (transaction->state() != State::StateType::BEGUN) {
    // TODO: set it to aborted
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  transaction->setState(State::StateType::ABORTED);

  // leave it in the list of transactions
  return TRI_ERROR_NO_ERROR;
}

// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

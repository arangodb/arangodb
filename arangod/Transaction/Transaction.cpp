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
                          TRI_vocbase_t* vocbase) 
  : _manager(manager),
    _id(id),
    _state(STATE_UNINITIALISED),
    _vocbase(vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction manager
////////////////////////////////////////////////////////////////////////////////

Transaction::~Transaction () {
  if (state() != STATE_COMMITTED && state() != STATE_ABORTED) {
    this->abort();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::begin () {
  if (state() == STATE_UNINITIALISED &&
      _manager->beginTransaction(this)) {
    _state = STATE_BEGUN;

    return TRI_ERROR_NO_ERROR;
  }

  this->abort();
  return TRI_ERROR_TRANSACTION_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////
        
int Transaction::commit () {
  if (state() == STATE_BEGUN && 
      _manager->commitTransaction(this)) {
    _state = STATE_COMMITTED;

    return TRI_ERROR_NO_ERROR;
  }
  
  this->abort();
  return TRI_ERROR_TRANSACTION_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////
        
int Transaction::abort () {
  if (state() == STATE_BEGUN &&
      _manager->abortTransaction(this)) {
    _state = STATE_ABORTED;

    return TRI_ERROR_NO_ERROR;
  }

  _state = STATE_ABORTED;
  return TRI_ERROR_TRANSACTION_INTERNAL;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

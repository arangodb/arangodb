////////////////////////////////////////////////////////////////////////////////
/// @brief Implementation part of generic transactions
///
/// @file Transaction.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Utils/transactions.h"

using namespace triagens::arango;

////////////////////////////////////////////////////////////////////////////////
/// @brief the following is for the runtime protection check, number of
/// transaction objects in scope in the current thread
////////////////////////////////////////////////////////////////////////////////

thread_local int TransactionBase::_numberTrxInScope = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief the following is for the runtime protection check, number of
/// transaction objects in the current thread that are active (between
/// begin and commit()/abort().
////////////////////////////////////////////////////////////////////////////////

thread_local int TransactionBase::_numberTrxActive = 0;

// -----------------------------------------------------------------------------
// --SECTION--
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:



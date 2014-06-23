////////////////////////////////////////////////////////////////////////////////
/// @brief transaction macros
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

#ifndef ARANGODB_TRANSACTION_TRANSACTIONS_H
#define ARANGODB_TRANSACTION_TRANSACTIONS_H 1

#include "Basics/Common.h"
#include "BasicsC/logging.h"
#include "Transaction/Context.h"
#include "Transaction/Helper.h"
#include "Transaction/Manager.h"
#include "Transaction/Transaction.h"
#include "Transaction/WorkUnit.h"
#include "Utils/Exception.h"
#include "Wal/LogfileManager.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief try block for a transaction
////////////////////////////////////////////////////////////////////////////////

#define TRANSACTION_TRY(vocbase, context, globalContext)                       \
  try {                                                                        \
    triagens::transaction::Manager* manager = new triagens::transaction::Manager(); \
    triagens::transaction::Context* context = triagens::transaction::Context::getContext(manager, triagens::wal::LogfileManager::instance(), vocbase, globalContext);

////////////////////////////////////////////////////////////////////////////////
/// @brief catch block for a transaction
////////////////////////////////////////////////////////////////////////////////

#define TRANSACTION_CATCH(ex)                                                  \
  }                                                                            \
  catch (triagens::arango::Exception const &ex) {                              \
    LOG_INFO("transaction exception: %s", ex.what());                          \


////////////////////////////////////////////////////////////////////////////////
/// @brief final block for a transaction
////////////////////////////////////////////////////////////////////////////////

#define TRANSACTION_FINALLY                                                    \
  }

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

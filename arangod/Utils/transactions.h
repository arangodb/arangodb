////////////////////////////////////////////////////////////////////////////////
/// @brief transctions helper file
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

#ifndef ARANGODB_UTILS_TRANSACTIONS_H
#define ARANGODB_UTILS_TRANSACTIONS_H 1

#include "Basics/Common.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExplicitTransaction.h"
#include "Utils/ReplicationTransaction.h"
#include "Utils/SingleCollectionReadOnlyTransaction.h"
#include "Utils/SingleCollectionWriteTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/V8TransactionContext.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for read-only transaction class type
////////////////////////////////////////////////////////////////////////////////

#define V8ReadTransaction triagens::arango::SingleCollectionReadOnlyTransaction

////////////////////////////////////////////////////////////////////////////////
/// @brief import transaction shortcut
////////////////////////////////////////////////////////////////////////////////

#define RestImportTransaction triagens::arango::SingleCollectionWriteTransaction<UINT64_MAX>

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for single collection, multi-operation write transactions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_UTILS_IMPORT_TRANSACTION_H
#define TRIAGENS_UTILS_IMPORT_TRANSACTION_H 1

#include "Utils/SingleCollectionWriteTransaction.h"

#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                           class ImportTransaction
// -----------------------------------------------------------------------------

    template<bool E>
    class ImportTransaction : public SingleCollectionWriteTransaction<E, UINT64_MAX> {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection object
///
/// An import transaction operates on a single collection and may execute any
/// number of writes on it.
////////////////////////////////////////////////////////////////////////////////

        ImportTransaction (TRI_vocbase_t* const vocbase,
                           TRI_transaction_t* previousTrx,
                           const string& collectionName, 
                           const TRI_col_type_e collectionType, 
                           const bool createCollection) :
          SingleCollectionWriteTransaction<E, UINT64_MAX>(vocbase, previousTrx, collectionName, collectionType, createCollection, "ImportTransaction") { 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~ImportTransaction () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:

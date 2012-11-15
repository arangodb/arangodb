////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for self-contained, single collection read transactions
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

#ifndef TRIAGENS_UTILS_SINGLE_COLLECTION_READ_ONLY_TRANSACTION_H
#define TRIAGENS_UTILS_SINGLE_COLLECTION_READ_ONLY_TRANSACTION_H 1

#include "Utils/SingleCollectionTransaction.h"

#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                         class SingleCollectionReadOnlyTransaction
// -----------------------------------------------------------------------------

    template<bool E>
    class SingleCollectionReadOnlyTransaction : public SingleCollectionTransaction<E> {

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
/// A self-contained read transaction is a transaction on a single collection
/// that only allows read operations. Write operations are not supported.
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionReadOnlyTransaction (TRI_vocbase_t* const vocbase,
                                             TRI_transaction_t* previousTrx,
                                             const string& collectionName, 
                                             const TRI_col_type_e collectionType) :
          SingleCollectionTransaction<E>(vocbase, previousTrx, collectionName, collectionType, false, "SingleCollectionReadOnlyTransaction", TRI_TRANSACTION_READ) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~SingleCollectionReadOnlyTransaction () {
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

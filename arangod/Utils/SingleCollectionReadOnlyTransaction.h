////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for self-contained, single collection read transactions
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

#ifndef TRIAGENS_UTILS_SINGLE_COLLECTION_READ_ONLY_TRANSACTION_H
#define TRIAGENS_UTILS_SINGLE_COLLECTION_READ_ONLY_TRANSACTION_H 1

#include "Utils/CollectionNameResolver.h"
#include "Utils/SingleCollectionTransaction.h"

#include "VocBase/transaction.h"

struct TRI_vocbase_s;

namespace triagens {
  namespace arango {

    template<typename T>
    class SingleCollectionReadOnlyTransaction : public SingleCollectionTransaction<T> {

// -----------------------------------------------------------------------------
// --SECTION--                         class SingleCollectionReadOnlyTransaction
// -----------------------------------------------------------------------------

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

        SingleCollectionReadOnlyTransaction (struct TRI_vocbase_s* const vocbase,
                                             const triagens::arango::CollectionNameResolver& resolver,
                                             const TRI_transaction_cid_t cid) :
          SingleCollectionTransaction<T>(vocbase, resolver, cid, TRI_TRANSACTION_READ) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief same as above, but create using collection name
////////////////////////////////////////////////////////////////////////////////

        SingleCollectionReadOnlyTransaction (struct TRI_vocbase_s* const vocbase,
                                             const triagens::arango::CollectionNameResolver& resolver,
                                             const string& name) :
          SingleCollectionTransaction<T>(vocbase, resolver, resolver.getCollectionId(name), TRI_TRANSACTION_READ) {
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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

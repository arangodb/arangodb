////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for single collection, single operation write transactions
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

#ifndef TRIAGENS_UTILS_SELF_CONTAINED_WRITE_TRANSACTION_H
#define TRIAGENS_UTILS_SELF_CONTAINED_WRITE_TRANSACTION_H 1

#include "Utils/SingleCollectionWriteTransaction.h"

#include "Utils/StandaloneTransaction.h"

#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace arango {

    template<typename C>
    class SelfContainedWriteTransaction : public SingleCollectionWriteTransaction<StandaloneTransaction<C>, 1> {

// -----------------------------------------------------------------------------
// --SECTION--                               class SelfContainedWriteTransaction
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
/// A self contained write transaction operates on a single collection and may 
/// execute at most one write operation
////////////////////////////////////////////////////////////////////////////////

        SelfContainedWriteTransaction (TRI_vocbase_t* const vocbase,
                                       const string& name) :
          SingleCollectionWriteTransaction<StandaloneTransaction<C>, 1>(vocbase, name) { 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection object
///
/// A self contained write transaction operates on a single collection and may 
/// execute at most one write operation
////////////////////////////////////////////////////////////////////////////////

        SelfContainedWriteTransaction (TRI_vocbase_t* const vocbase,
                                       const string& name,
                                       const TRI_col_type_e createType) :
          SingleCollectionWriteTransaction<StandaloneTransaction<C>, 1>(vocbase, name, createType) { 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create the transaction, using a collection object
///
/// A self contained write transaction operates on a single collection and may 
/// execute at most one write operation
////////////////////////////////////////////////////////////////////////////////

        SelfContainedWriteTransaction (TRI_vocbase_t* const vocbase,
                                       const string& name,
                                       const bool create,
                                       const TRI_col_type_e createType) :
          SingleCollectionWriteTransaction<StandaloneTransaction<C>, 1>(vocbase, name, create, createType) { 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~SelfContainedWriteTransaction () {
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

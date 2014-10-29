////////////////////////////////////////////////////////////////////////////////
/// @brief standalone transaction context
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

#ifndef ARANGODB_UTILS_STANDALONE_TRANSACTION_CONTEXT_H
#define ARANGODB_UTILS_STANDALONE_TRANSACTION_CONTEXT_H 1

#include "Basics/Common.h"

#include "VocBase/transaction.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/TransactionContext.h"

struct TRI_transaction_s;

namespace triagens {
  namespace arango {

    class StandaloneTransactionContext : public TransactionContext {

// -----------------------------------------------------------------------------
// --SECTION--                                class StandaloneTransactionContext
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the context
////////////////////////////////////////////////////////////////////////////////

        StandaloneTransactionContext (); 

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the context
////////////////////////////////////////////////////////////////////////////////

        ~StandaloneTransactionContext ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the resolver
////////////////////////////////////////////////////////////////////////////////

        CollectionNameResolver const* getResolver () const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the parent transaction (none in our case)
////////////////////////////////////////////////////////////////////////////////
        
        struct TRI_transaction_s* getParentTransaction () const override;

////////////////////////////////////////////////////////////////////////////////
/// @brief register the transaction, does nothing
////////////////////////////////////////////////////////////////////////////////

        int registerTransaction (struct TRI_transaction_s*) override;

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the transaction, does nothing
////////////////////////////////////////////////////////////////////////////////

        int unregisterTransaction () override;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is embeddable
////////////////////////////////////////////////////////////////////////////////

        bool isEmbeddable () const override;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief collection name resolver
////////////////////////////////////////////////////////////////////////////////

        CollectionNameResolver* _resolver;

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

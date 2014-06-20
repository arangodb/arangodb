////////////////////////////////////////////////////////////////////////////////
/// @brief rest transaction context
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

#ifndef TRIAGENS_UTILS_REST_TRANSACTION_CONTEXT_H
#define TRIAGENS_UTILS_REST_TRANSACTION_CONTEXT_H 1

#include "Basics/Common.h"

#include "VocBase/transaction.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"

namespace triagens {
  namespace arango {

    class RestTransactionContext {

// -----------------------------------------------------------------------------
// --SECTION--                                      class RestTransactionContext
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the context
////////////////////////////////////////////////////////////////////////////////

        RestTransactionContext () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the context
////////////////////////////////////////////////////////////////////////////////

        virtual ~RestTransactionContext () {
//          unregisterTransaction();
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the resolver
////////////////////////////////////////////////////////////////////////////////

        inline CollectionNameResolver const* getResolver () const {
          return _resolver;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is embeddable
////////////////////////////////////////////////////////////////////////////////

        inline bool isEmbeddable () {
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the parent transaction (none in our case)
////////////////////////////////////////////////////////////////////////////////

        inline TRI_transaction_t* getParentTransaction () const {
          return nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief register the transaction, does nothing
////////////////////////////////////////////////////////////////////////////////

        inline int registerTransaction (TRI_transaction_t* trx) {
          _resolver = new CollectionNameResolver(trx->_vocbase);

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the transaction, does nothing
////////////////////////////////////////////////////////////////////////////////

        inline int unregisterTransaction () {
          if (_resolver != nullptr) {
            delete _resolver;
            _resolver = nullptr;
          }

          return TRI_ERROR_NO_ERROR;
        }

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

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 transaction context
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

#ifndef ARANGODB_UTILS_V8TRANSACTION_CONTEXT_H
#define ARANGODB_UTILS_V8TRANSACTION_CONTEXT_H 1

#include "Basics/Common.h"

#include <v8.h>

#include "V8/v8-globals.h"

#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"

namespace triagens {
  namespace arango {

    template<bool EMBEDDABLE>
    class V8TransactionContext {

// -----------------------------------------------------------------------------
// --SECTION--                                        class V8TransactionContext
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the context
////////////////////////////////////////////////////////////////////////////////

        V8TransactionContext ()
          : _v8g(static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData())),
            _ownResolver(false) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the context
////////////////////////////////////////////////////////////////////////////////

        virtual ~V8TransactionContext () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the resolver
////////////////////////////////////////////////////////////////////////////////

        inline CollectionNameResolver const* getResolver () const { 
          TRI_ASSERT(_v8g != nullptr);
          TRI_ASSERT_EXPENSIVE(_v8g->_resolver != nullptr);
          return static_cast<CollectionNameResolver*>(_v8g->_resolver);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the transaction is embedded
////////////////////////////////////////////////////////////////////////////////

        static inline bool IsEmbedded () {
          TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
          return (v8g->_currentTransaction != nullptr);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is embeddable
////////////////////////////////////////////////////////////////////////////////

        inline bool isEmbeddable () const {
          return EMBEDDABLE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get parent transaction (if any)
////////////////////////////////////////////////////////////////////////////////

        inline TRI_transaction_t* getParentTransaction () const {
          TRI_ASSERT(_v8g != nullptr);
          if (_v8g->_currentTransaction != nullptr) {
            return static_cast<TRI_transaction_t*>(_v8g->_currentTransaction);
          }

          return nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief register the transaction in the context
////////////////////////////////////////////////////////////////////////////////

        inline int registerTransaction (TRI_transaction_t* trx) {
          TRI_ASSERT(_v8g != nullptr);
          TRI_ASSERT_EXPENSIVE(_v8g->_currentTransaction == nullptr);
          _v8g->_currentTransaction = trx;

          if (_v8g->_resolver == nullptr) {
            _v8g->_resolver = static_cast<void*>(new CollectionNameResolver(trx->_vocbase));
            _ownResolver = true;
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the transaction from the context
////////////////////////////////////////////////////////////////////////////////

        inline int unregisterTransaction () {
          TRI_ASSERT(_v8g != nullptr);
          _v8g->_currentTransaction = nullptr;

          if (_ownResolver && _v8g->_resolver != nullptr) {
            _ownResolver = false;
            CollectionNameResolver* resolver = static_cast<CollectionNameResolver*>(_v8g->_resolver);
            delete resolver;

            _v8g->_resolver = nullptr;
          }

          return TRI_ERROR_NO_ERROR;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief v8 global context
////////////////////////////////////////////////////////////////////////////////

        TRI_v8_global_t* _v8g;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not we are responsible for the resolver
////////////////////////////////////////////////////////////////////////////////

        bool _ownResolver;

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

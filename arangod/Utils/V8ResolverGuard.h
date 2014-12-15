////////////////////////////////////////////////////////////////////////////////
/// @brief V8 collection name resolver guard
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

#ifndef ARANGODB_UTILS_V8RESOLVER_GUARD_H
#define ARANGODB_UTILS_V8RESOLVER_GUARD_H 1

#include "Basics/Common.h"

#include "Utils/CollectionNameResolver.h"
#include "Utils/V8TransactionContext.h"
#include "VocBase/vocbase.h"
#include "V8/v8-globals.h"
#include <v8.h>

namespace triagens {
  namespace arango {

    class V8ResolverGuard {

// -----------------------------------------------------------------------------
// --SECTION--                                             class V8ResolverGuard
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the guard
////////////////////////////////////////////////////////////////////////////////

        V8ResolverGuard (TRI_vocbase_t* vocbase)
          : _v8g(static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData(V8DataSlot))),
            _ownResolver(false) {

          if (! static_cast<V8TransactionContext*>(_v8g->_transactionContext)->hasResolver()) {
            static_cast<V8TransactionContext*>(_v8g->_transactionContext)->setResolver(new CollectionNameResolver(vocbase));
            _ownResolver = true;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the guard
////////////////////////////////////////////////////////////////////////////////

        ~V8ResolverGuard () {
          if (_ownResolver && static_cast<V8TransactionContext*>(_v8g->_transactionContext)->hasResolver()) {
            static_cast<V8TransactionContext*>(_v8g->_transactionContext)->deleteResolver();
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the resolver
////////////////////////////////////////////////////////////////////////////////

        inline CollectionNameResolver const* getResolver () const { 
          TRI_ASSERT_EXPENSIVE(static_cast<V8TransactionContext*>(_v8g->_transactionContext)->hasResolver());
          return static_cast<V8TransactionContext*>(_v8g->_transactionContext)->getResolver();
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

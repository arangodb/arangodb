////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for potentially embedded transactions
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

#ifndef TRIAGENS_UTILS_EMBEDDABLE_TRANSACTION_H
#define TRIAGENS_UTILS_EMBEDDABLE_TRANSACTION_H 1

#include "VocBase/transaction.h"

#include "V8/v8-globals.h"

#include <v8.h>

namespace triagens {
  namespace arango {

    template<typename C>
    class EmbeddableTransaction : public C {

// -----------------------------------------------------------------------------
// --SECTION--                                       class EmbeddableTransaction
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
/// @brief create the transaction
////////////////////////////////////////////////////////////////////////////////

        EmbeddableTransaction () : _trx(0) {
          TRI_v8_global_t* v8g;

          v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

          if (v8g->_currentTransaction == 0) {
            _previous = (TRI_transaction_t*) v8g->_currentTransaction;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~EmbeddableTransaction () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
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
/// @brief whether or not the transaction is embeddable
////////////////////////////////////////////////////////////////////////////////

        inline bool isEmbeddable () const {
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief free the transaction
////////////////////////////////////////////////////////////////////////////////

        int freeTransaction () {
          if (this->isEmbedded()) {
            return TRI_ERROR_NO_ERROR;
          }

          if (_trx != 0) {
            this->unregisterTransaction();

            TRI_FreeTransaction(_trx);
            _trx = 0;
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief previous transaction, if any
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_t* _previous;

////////////////////////////////////////////////////////////////////////////////
/// @brief used transaction
////////////////////////////////////////////////////////////////////////////////

        TRI_transaction_t* _trx;

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

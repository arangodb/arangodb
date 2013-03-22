////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for explicit transactions
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

#ifndef TRIAGENS_UTILS_USER_TRANSACTION_H
#define TRIAGENS_UTILS_USER_TRANSACTION_H 1

#include "Utils/Transaction.h"

#include "VocBase/transaction.h"

struct TRI_vocbase_s;

namespace triagens {
  namespace arango {

    template<typename T>
    class ExplicitTransaction : public Transaction<T> {

// -----------------------------------------------------------------------------
// --SECTION--                                         class ExplicitTransaction
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

        ExplicitTransaction (struct TRI_vocbase_s* const vocbase,
                             const triagens::arango::CollectionNameResolver& resolver,
                             const vector<string>& readCollections,
                             const vector<string>& writeCollections) :
          Transaction<T>(vocbase, resolver) {

          this->addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY);

          for (size_t i = 0; i < readCollections.size(); ++i) {
            this->addCollection(readCollections[i], TRI_TRANSACTION_READ);
          }

          for (size_t i = 0; i < writeCollections.size(); ++i) {
            this->addCollection(writeCollections[i], TRI_TRANSACTION_WRITE);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief end the transaction
////////////////////////////////////////////////////////////////////////////////

        ~ExplicitTransaction () {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction id generator
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

#ifndef TRIAGENS_TRANSACTION_IDGENERATOR_H
#define TRIAGENS_TRANSACTION_IDGENERATOR_H 1

#include "Basics/Common.h"
#include "Transaction/Transaction.h"

namespace triagens {
  namespace transaction {

// -----------------------------------------------------------------------------
// --SECTION--                                                 class IdGenerator
// -----------------------------------------------------------------------------

    class IdGenerator {

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction
////////////////////////////////////////////////////////////////////////////////

      private:
        IdGenerator (IdGenerator const&);
        IdGenerator& operator= (IdGenerator const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create an id generator
////////////////////////////////////////////////////////////////////////////////

        IdGenerator (Transaction::IdType = 0);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an id generator
////////////////////////////////////////////////////////////////////////////////

        ~IdGenerator ();

////////////////////////////////////////////////////////////////////////////////
/// @brief set minimal transaction id
////////////////////////////////////////////////////////////////////////////////

        void setLastId (Transaction::IdType id);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction id
////////////////////////////////////////////////////////////////////////////////

        Transaction::IdType next ();

    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

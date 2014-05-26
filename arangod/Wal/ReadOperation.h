////////////////////////////////////////////////////////////////////////////////
/// @brief WAL read operation
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

#ifndef TRIAGENS_WAL_READ_OPERATION_H
#define TRIAGENS_XWAL_READ_OPERATION_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace wal {

// -----------------------------------------------------------------------------
// --SECTION--                                               class ReadOperation
// -----------------------------------------------------------------------------

    class ReadOperation {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create a read operation
////////////////////////////////////////////////////////////////////////////////

        explicit ReadOperation (void**);
        
        ReadOperation ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a read operation
////////////////////////////////////////////////////////////////////////////////

        ~ReadOperation ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        ReadOperation** _address;

        uint64_t _id;


    };

  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

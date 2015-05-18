////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, short string storage
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_SHORT_STRING_STORAGE_H
#define ARANGODB_AQL_SHORT_STRING_STORAGE_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                          class ShortStringStorage
// -----------------------------------------------------------------------------

    class ShortStringStorage {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
 
      public:

        ShortStringStorage (ShortStringStorage const&) = delete;
        ShortStringStorage& operator= (ShortStringStorage const&) = delete;
     
        explicit ShortStringStorage (size_t);
        
        ~ShortStringStorage ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief register a short string
////////////////////////////////////////////////////////////////////////////////
 
        char* registerString (char const*, 
                              size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate a new block of memory
////////////////////////////////////////////////////////////////////////////////

        void allocateBlock ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum length of strings in short string storage
////////////////////////////////////////////////////////////////////////////////

        static size_t const MaxStringLength;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief already allocated string blocks
////////////////////////////////////////////////////////////////////////////////

        std::vector<char*> _blocks;

////////////////////////////////////////////////////////////////////////////////
/// @brief size of each block
////////////////////////////////////////////////////////////////////////////////

        size_t const _blockSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief offset into current block
////////////////////////////////////////////////////////////////////////////////

        char* _current;

////////////////////////////////////////////////////////////////////////////////
/// @brief end of current block
////////////////////////////////////////////////////////////////////////////////

        char* _end;
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

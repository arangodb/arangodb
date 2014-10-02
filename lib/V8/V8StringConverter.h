////////////////////////////////////////////////////////////////////////////////
/// @brief V8 string converter
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

#ifndef ARANGODB_V8_V8_STRING_CONVERTER_H
#define ARANGODB_V8_V8_STRING_CONVERTER_H 1

#include "Basics/Common.h"

#include <v8.h>

struct UNormalizer2;

namespace triagens {
  namespace utils {

    class V8StringConverter {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
      
      public: 

////////////////////////////////////////////////////////////////////////////////
/// @brief create the converter
////////////////////////////////////////////////////////////////////////////////

        V8StringConverter (TRI_memory_zone_t*); 

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the converter
////////////////////////////////////////////////////////////////////////////////

        ~V8StringConverter ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public: 

////////////////////////////////////////////////////////////////////////////////
/// @brief assign a V8 string to the converter and convert it
////////////////////////////////////////////////////////////////////////////////

        bool assign (v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief steal the string
////////////////////////////////////////////////////////////////////////////////

        inline char* steal () {
          char* tmp = _str;
          _str = nullptr;
          _strCapacity = 0;
          return tmp;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the length of the converted string
////////////////////////////////////////////////////////////////////////////////

        inline size_t length () const {
          return _length;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief reserve a buffer for operations
////////////////////////////////////////////////////////////////////////////////

        bool reserveBuffer (char*&,
                            size_t&,
                            size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief buffer for the converted string
////////////////////////////////////////////////////////////////////////////////

        char* _str;

////////////////////////////////////////////////////////////////////////////////
/// @brief temporary buffer for Unicode conversion
////////////////////////////////////////////////////////////////////////////////

        char* _temp;

////////////////////////////////////////////////////////////////////////////////
/// @brief length of _str
////////////////////////////////////////////////////////////////////////////////

        size_t _length;

////////////////////////////////////////////////////////////////////////////////
/// @brief size of _str buffer (allocated size)
////////////////////////////////////////////////////////////////////////////////

        size_t _strCapacity;

////////////////////////////////////////////////////////////////////////////////
/// @brief size of _temp buffer (allocated size)
////////////////////////////////////////////////////////////////////////////////

        size_t _tempCapacity;

////////////////////////////////////////////////////////////////////////////////
/// @brief memory zone
////////////////////////////////////////////////////////////////////////////////

        TRI_memory_zone_t* _memoryZone;

////////////////////////////////////////////////////////////////////////////////
/// @brief UTF-8 normalizer instance
////////////////////////////////////////////////////////////////////////////////

        UNormalizer2 const* _normalizer;
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

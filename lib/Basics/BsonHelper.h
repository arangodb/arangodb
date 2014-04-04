////////////////////////////////////////////////////////////////////////////////
/// @brief abstraction for libbson
///
/// @file lib/Basics/BsonHelper.h
///
/// DISCLAIMER
///
/// Copyright 2014-2014 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_BSON_HELPER_H
#define TRIAGENS_BASICS_BSON_HELPER_H 1

extern "C" {
#include "libbson-1.0/bson.h"
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            types
// -----------------------------------------------------------------------------

namespace triagens {
  namespace basics {

    class Bson {

// -----------------------------------------------------------------------------
// --SECTION--                                                     private data
// -----------------------------------------------------------------------------

        bson_t _bson;        // this class is only a very thin wrapping layer

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief Create an empty Bson structure
////////////////////////////////////////////////////////////////////////////////

        Bson () {
          bson_init(&_bson);
        }

        Bson (uint8_t const* data, uint32_t length) {
          bson_init_static(&_bson, data, length);
        }

        ~Bson () {
          bson_destroy(&_bson);
        }

        uint8_t const* getBuffer () {
          return bson_data(&_bson);
        }

        uint8_t* steal (uint32_t* length) {
          uint8_t* buf = bson_destroy_with_steal(&_bson, true, length);
          bson_init(&_bson);
        }

        void clear () {
          bson_reinit(&_bson);
        }

        
        

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:



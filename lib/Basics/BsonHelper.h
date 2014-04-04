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
        uint32_t count;      // for array appending

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief Create an empty Bson structure
////////////////////////////////////////////////////////////////////////////////

        void init () {
          bson_init(&_bson);
          count = 0;
        }

        void destruct () {
          bson_destroy(&_bson);
        }

        Bson () {
          // This will be empty but can mutable.
          init();
        }

        Bson (uint8_t const* data, uint32_t length) {
          // This will be readonly!
          bson_init_static(&_bson, data, length);
          count = 0;
        }

        ~Bson () {
          destruct();
        }

        uint8_t const* getBuffer () {
          // Only usable to copy out.
          return bson_get_data(&_bson);
        }

        uint8_t* steal (uint32_t* length) {
          // Ownership goes over to the caller.
          uint8_t* buf = bson_destroy_with_steal(&_bson, true, length);
          bson_init(&_bson);
          return buf;
        }

        void clear () {
          bson_reinit(&_bson);
          count = 0;
        }

        bool append_undefined (string const key) {
          // Returns false if append did not work.
          return bson_append_undefined(&_bson, key.c_str(), key.size());
        }

        bool append_null (string const key) {
          // Returns false if append did not work.
          return bson_append_null(&_bson, key.c_str(), key.size());
        }

        bool append_bool (string const key, bool value) {
          // Returns false if append did not work.
          return bson_append_bool(&_bson, key.c_str(), key.size(), value);
        }

        bool append_double (string const key, double value) {
          // Returns false if append did not work.
          return bson_append_double(&_bson, key.c_str(), key.size(), value);
        }

        bool append_utf8 (string const key, string value) {
          // Returns false if append did not work.
          return bson_append_utf8(&_bson, key.c_str(), key.size(), 
                                          value.c_str(), value.size());
        }

        bool append_document_begin (string const key, Bson& child) {
          // Returns false if append did not work, child will be
          // destroyed and reinitialised, even if the append operation
          // did not work.
          bool result;
          child.destruct();
          result = bson_append_document_begin(&_bson, key.c_str(), key.size(),
                                              &child._bson);
          if (result) {
            child.count = 0;
            return true;
          }
          else {
            child.init();
            return false;
          }
        }

        bool append_document_end (string const key, Bson& child) {
          // Returns false if append did not work, child will be
          // empty afterwards, even if the append operation did not work.
          bool result;
          result = bson_append_document_end(&_bson, &child._bson);
          child.init();
          return result;
        }


    };   // class Bson

  }  // namespace triagens.basics
}  // namespace triagens
#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:



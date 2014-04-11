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

#include "Common.h"
#include "Basics/StringUtils.h"
#include "BasicsC/json.h"
#include "BasicsC/vector.h"

extern "C" {
#include "libbson-1.0/bson.h"
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            types
// -----------------------------------------------------------------------------

namespace triagens {
  namespace basics {

    class BsonIter;

    class Bson {

        friend class BsonIter;

// -----------------------------------------------------------------------------
// --SECTION--                                                     private data
// -----------------------------------------------------------------------------

        bson_t _bson;        // this class is only a very thin wrapping layer
        uint32_t count;      // for array appending

        void init () {
          bson_init(&_bson);
          count = 0;
        }

        void destruct () {
          bson_destroy(&_bson);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief Create an empty Bson structure
////////////////////////////////////////////////////////////////////////////////

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

        Bson (Bson const& another) {
          bson_copy_to(&another._bson, &_bson);
          count = 0;
        }

        void operator= (Bson const& that) {
          destruct();
          bson_copy_to(&that._bson, &_bson);
          count = 0;
        }

        uint8_t const* getBuffer () const {
          // Only usable to copy out.
          return bson_get_data(&_bson);
        }
        
        uint32_t getSize () const {
          return _bson.len;
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

        bool appendNull (string const key) {
          // Returns false if append did not work.
          return bson_append_null(&_bson, key.c_str(), key.size());
        }

        bool appendBool (string const key, bool value) {
          // Returns false if append did not work.
          return bson_append_bool(&_bson, key.c_str(), key.size(), value);
        }

        bool appendDouble (string const key, double value) {
          // Returns false if append did not work.
          return bson_append_double(&_bson, key.c_str(), key.size(), value);
        }

        bool appendUtf8 (string const key, string value) {
          // Returns false if append did not work.
          return bson_append_utf8(&_bson, key.c_str(), key.size(), 
                                          value.c_str(), value.size());
        }
        
        bool appendArrayBegin (string const key, Bson& child) {
          // Returns false if append did not work, child will be
          // destroyed and reinitialised, even if the append operation
          // did not work.
          bool result;
          child.destruct();
          result = bson_append_array_begin(&_bson, key.c_str(), key.size(),
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
        
        bool appendArrayEnd (Bson& child) {
          // Returns false if append did not work, child will be
          // empty afterwards, even if the append operation did not work.
          bool result;
          result = bson_append_array_end(&_bson, &child._bson);
          child.init();
          return result;
        }

        bool appendDocumentBegin (string const key, Bson& child) {
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

        bool appendDocumentEnd (Bson& child) {
          // Returns false if append did not work, child will be
          // empty afterwards, even if the append operation did not work.
          bool result;
          result = bson_append_document_end(&_bson, &child._bson);
          child.init();
          return result;
        }

        bool processJsonPart (TRI_json_t const* json) {
          assert(json->_type == TRI_JSON_LIST ||
                 json->_type == TRI_JSON_ARRAY);

          size_t step;
          if (json->_type == TRI_JSON_LIST) {
            step = 1;
          }
          else {
            step = 2;
          }

          for (size_t i = 0; i < json->_value._objects._length; i += step) {
            TRI_json_t const* key;
            TRI_json_t const* value;
            string keyString;

            if (step == 1) {
              // list
              value = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
              
              keyString = StringUtils::itoa(i);
            }
            else {
              // array
              key   = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
              value = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i + 1));
              keyString = string(key->_value._string.data, key->_value._string.length - 1);
            }
                
            switch (value->_type) {
              case TRI_JSON_UNUSED: 
              case TRI_JSON_NULL: {
                if (! appendNull(keyString)) {
                  return false;
                }
                break;
              }

              case TRI_JSON_BOOLEAN: {
                if (! appendBool(keyString, value->_value._boolean)) {
                  return false;
                }
                break;
              }

              case TRI_JSON_NUMBER: {
                if (! appendDouble(keyString, value->_value._number)) {
                  return false;
                }
                break;
              }
              
              case TRI_JSON_STRING: 
              case TRI_JSON_STRING_REFERENCE: {
                if (! appendUtf8(keyString, string(value->_value._string.data, value->_value._string.length - 1))) {
                  return false;
                }
                break;
              }
              
              case TRI_JSON_LIST: {
                Bson child;
                appendArrayBegin(keyString, child);

                if (! processJsonPart(value)) {
                  return false;
                }

                appendArrayEnd(child);
                break;
              }
              
              case TRI_JSON_ARRAY: {
                Bson child;
                appendDocumentBegin(keyString, child);

                if (! processJsonPart(value)) {
                  return false;
                }

                appendDocumentEnd(child);
                break;
              }
            }
          }

          return true;
        }

        bool fromJson (string const& value) {
          TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, value.c_str());  

          if (json == NULL) {
            return false;
          }

          if (json->_type != TRI_JSON_ARRAY) {
            // wrong type. must be document
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
            return false;
          }

          clear();

          processJsonPart(json);

          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          return true;
        }

        bool toJson (string& result) {
          size_t length;
          char* p = bson_as_json(&_bson, &length);
          if (NULL == p) {
            return false;
          }
          result.clear();
          result.append(p, length);
          bson_free(p);
          return true;
        }

        bool operator== (Bson const& that) {
          return bson_equal(&_bson, &that._bson);
        }

        bool operator< (Bson const& that) {
          return bson_compare(&_bson, &that._bson) == -1;
        }

        bool operator<= (Bson const& that) {
          return bson_compare(&_bson, &that._bson) <= 0;
        }

        bool operator>= (Bson const& that) {
          return bson_compare(&_bson, &that._bson) >= 0;
        }

        int compare (Bson const& that) {
          return bson_compare(&_bson, &that._bson);
        }

        bool appendIterItem (string key, BsonIter& that);
        // This needs to be a real method because it needs access to
        // the details of the BsonIter class.

        bool appendBson (Bson const& that) {
          return bson_concat(&_bson, &that._bson);
        }

        size_t size () {
          return static_cast<size_t>(bson_count_keys(&_bson));
        }

        bool hasField (string key) {
          return bson_has_field(&_bson, key.c_str());
        }
    };   // class Bson

    class BsonIter {

        friend class Bson;

        bson_iter_t _bsonIter;
        bool _hasData;
      
      public:

        BsonIter (Bson const& b) : _hasData(false) {
          bson_iter_init (&_bsonIter, &(b._bson));
        }

        ~BsonIter () {}    // no destruction is needed for bson_iter_t objects 

        bool hasData () {
          return _hasData;
        }

        bool next () {
          // Must be called before accessing the first element, returns
          // true iff there is another one after this call.
          return (_hasData = bson_iter_next(&_bsonIter));
        }

        bool find (string const key) {
          // Returns true if key is found. Note: case-insensitive.
          return (_hasData = bson_iter_find(&_bsonIter, key.c_str()));
        }

        bool findCaseSensitive (string const key) {
          // Returns true if key is found.
          return (_hasData = bson_iter_find(&_bsonIter, key.c_str()));
        }

        bool getKey (string& key) {
          // Copies key to the argument and returns true, if the
          // iterator is pointing to something, otherwise it returns
          // false and the argument is unchanged.
          if (! _hasData) {
            return false;
          }
          char const* p = bson_iter_key(&_bsonIter);
          key.clear();
          key.append(p);
          return true;
        }

        bson_type_t getType () {
          // Returns one of the allowed bson types:
          //  BSON_TYPE_EOD           = 0x00,
          //  BSON_TYPE_DOUBLE        = 0x01,
          //  BSON_TYPE_UTF8          = 0x02,
          //  BSON_TYPE_DOCUMENT      = 0x03,
          //  BSON_TYPE_ARRAY         = 0x04,
          //  BSON_TYPE_BINARY        = 0x05,
          //  BSON_TYPE_UNDEFINED     = 0x06,
          //  BSON_TYPE_OID           = 0x07,
          //  BSON_TYPE_BOOL          = 0x08,
          //  BSON_TYPE_DATE_TIME     = 0x09,
          //  BSON_TYPE_NULL          = 0x0A,
          //  BSON_TYPE_REGEX         = 0x0B,
          //  BSON_TYPE_DBPOINTER     = 0x0C,
          //  BSON_TYPE_CODE          = 0x0D,
          //  BSON_TYPE_SYMBOL        = 0x0E,
          //  BSON_TYPE_CODEWSCOPE    = 0x0F,
          //  BSON_TYPE_INT32         = 0x10,
          //  BSON_TYPE_TIMESTAMP     = 0x11,
          //  BSON_TYPE_INT64         = 0x12,
          //  BSON_TYPE_MAXKEY        = 0x7F,
          //  BSON_TYPE_MINKEY        = 0xFF,
          // of type bson_type_t.
          return (! _hasData) ? BSON_TYPE_EOD  
                              : bson_iter_type(&_bsonIter);
        }

        bool getBool () {
          return (! _hasData || ! BSON_ITER_HOLDS_BOOL(&_bsonIter))
                 ? false : bson_iter_bool(&_bsonIter);
        }

        double getDouble () {
          return (! _hasData || ! BSON_ITER_HOLDS_DOUBLE(&_bsonIter))
                 ? 0.0 : bson_iter_bool(&_bsonIter);
        }
         
        string getUtf8 () {
          string res;
          if (! _hasData || !  BSON_ITER_HOLDS_UTF8(&_bsonIter)) {
            return res;
          }
          uint32_t length;
          char const* p = bson_iter_utf8(&_bsonIter, &length);
          res.append(p,length);
          return res;
        }

        bool recurse (BsonIter& child) {
          bson_type_t type = getType();
          if (type == BSON_TYPE_ARRAY || type == BSON_TYPE_DOCUMENT) {
            child._hasData = false;
            return bson_iter_recurse(&_bsonIter, &child._bsonIter);
          }
          return false;
        }
    };

    inline string EscapeUtf8ForJson (string s) {
      char* p = bson_utf8_escape_for_json (s.c_str(), s.size());
      string res(p);
      bson_free(p);
      return res;
    }

    inline bool Bson::appendIterItem (string key, BsonIter& that) {
      if (! that.hasData()) {
        return false;
      }
      return bson_append_iter(&_bson, key.c_str(), key.size(), 
                              &that._bsonIter);
    }

    // Left out so far:
    //  append for anything non-JSON
    //  bson_writer
    //  bson_reader
    //  bson_context because it is only needed for oids
    //  copy_to_excluding
    //  extract raw sub-bsons from iter (bson_iter_array, bson_iter_document) 
    //  extract for anything non-JSON
    //  overwrite for fixed-length data via iterator
    //  visitor for bson
    //  oid support
    //  reserve: not exported by official libbson 
    //  bson-strings : unnecessary
    //  utf8-validation

  }  // namespace triagens.basics
}  // namespace triagens
#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:



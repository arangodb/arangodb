////////////////////////////////////////////////////////////////////////////////
/// @brief associative array for character pointer to POD
///
/// @file
/// The file implements an associative array, where the keys are character
/// pointers. It is the responsibility of the caller to clear the key
/// and values.
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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_DICTIONARY_H
#define TRIAGENS_BASICS_DICTIONARY_H 1

#include "Basics/Common.h"

#include "Basics/AssociativeArray.h"
#include "BasicsC/hashes.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                  class Dictionary
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array for character pointer to POD
////////////////////////////////////////////////////////////////////////////////

    template <typename ELEMENT>
    class Dictionary {
      private:
        Dictionary (Dictionary const&);
        Dictionary& operator= (Dictionary const&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief key-value stored in the dictonary
////////////////////////////////////////////////////////////////////////////////

        struct KeyValue {
          public:
            KeyValue ()
              : _key(0), _keyLength(0), _value() {
            }

            KeyValue (char const* key, size_t keyLength, ELEMENT const& value)
              : _key(key), _keyLength(keyLength), _value(value) {
            }

            KeyValue (char const* key, size_t keyLength)
              : _key(key), _keyLength(keyLength), _value() {
            }

          public:
            char const* _key;
            size_t _keyLength;
            ELEMENT _value;
        };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief description for the associative array
////////////////////////////////////////////////////////////////////////////////

        struct DictionaryDescription {
          public:
            static void clearElement (KeyValue& element) {
              element._key = 0;
            }

            static bool isEmptyElement (KeyValue const& element) {
              return element._key == 0;
            }

            static bool isEqualElementElement (KeyValue const& left, KeyValue const& right) {
              return left._keyLength == right._keyLength && memcmp(left._key, right._key, left._keyLength) == 0;
            }

            static uint32_t hashElement (KeyValue const& element) {
              return TRI_FnvHashPointer(element._key, element._keyLength) & 0xFFFFFFFF;
            }
        };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new associative array for POD data
////////////////////////////////////////////////////////////////////////////////

        explicit
        Dictionary (uint64_t size) :
          _array(size) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief swaps two dictonaries
////////////////////////////////////////////////////////////////////////////////

        void swap (Dictionary * other) {
          other->_array.swap(&_array);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key value pair
////////////////////////////////////////////////////////////////////////////////

        bool insert (char const* key, ELEMENT const& value) {
          KeyValue p(key, strlen(key), value);

          return _array.addElement(p);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a key value pair
////////////////////////////////////////////////////////////////////////////////

        bool insert (char const* key, size_t keyLength, ELEMENT const& value) {
          KeyValue p(key, keyLength, value);

          return _array.addElement(p);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a key
////////////////////////////////////////////////////////////////////////////////

        void erase (char const* key) {
          KeyValue l(key, strlen(key));
          _array.removeElement(l);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current range
////////////////////////////////////////////////////////////////////////////////

        void range (KeyValue const*& begin, KeyValue const*& end) const {
          size_t size;

          begin = _array.tableAndSize(size);
          end = begin + size;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a key
////////////////////////////////////////////////////////////////////////////////

        KeyValue const* lookup (char const* key) const {
          KeyValue l(key, strlen(key));
          KeyValue const& f = _array.findElement(l);

          if (f._key != 0 && strcmp(f._key, key) == 0) {
            return &f;
          }
          else {
            return 0;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a key
////////////////////////////////////////////////////////////////////////////////

        KeyValue const* lookup (char const* key, const size_t keyLength) const {
          KeyValue l(key, keyLength);
          KeyValue const& f = _array.findElement(l);

          if (f._key != 0 && strcmp(f._key, key) == 0) {
            return &f;
          }
          else {
            return 0;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying associative array
////////////////////////////////////////////////////////////////////////////////

        AssociativeArray<char const*, KeyValue, DictionaryDescription> _array;
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

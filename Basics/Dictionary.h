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
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2006-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_JUTLAND_BASICS_DICTIONARY_H
#define TRIAGENS_JUTLAND_BASICS_DICTIONARY_H 1

#include <Basics/Common.h>

#include <Basics/AssociativeArray.h>
#include <BasicsC/hashes.h>

namespace triagens {
  namespace basics {

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Utilities
    /// @brief associative array for character pointer to POD
    ////////////////////////////////////////////////////////////////////////////////

    template <typename ELEMENT>
    class Dictionary {
      private:
        Dictionary (Dictionary const&);
        Dictionary& operator= (Dictionary const&);

      public:
        struct KeyValue {
          public:
            KeyValue () :
              key(0), keyLength(0) {
            }

            KeyValue (char const* key, size_t keyLength, ELEMENT const& value) :
              key(key), keyLength(keyLength), value(value) {
            }

            KeyValue (char const* key, size_t keyLength) :
              key(key), keyLength(keyLength) {
            }

          public:
            char const* key;
            size_t keyLength;
            ELEMENT value;
        };

      private:
        struct DictionaryDescription {
          public:
            static void clearElement (KeyValue& element) {
              element.key = 0;
            }

            static bool isEmptyElement (KeyValue const& element) {
              return element.key == 0;
            }

            static bool isEqualElementElement (KeyValue const& left, KeyValue const& right) {
              return left.keyLength == right.keyLength && memcmp(left.key, right.key, left.keyLength) == 0;
            }

            static uint32_t hashElement (KeyValue const& element) {
              uint32_t hash;

              hash = TRI_InitialCrc32();
              hash = TRI_BlockCrc32(hash, reinterpret_cast<char const*> (element.key), element.keyLength);
              hash = TRI_FinalCrc32(hash);

              return hash;
            }
        };

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new associative array for POD data
        ////////////////////////////////////////////////////////////////////////////////

        explicit Dictionary (uint64_t size) :
          _array(size) {
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief swaps two dictonaries
        ////////////////////////////////////////////////////////////////////////////////

        void swap (Dictionary * other) {
          other->_array.swap(&_array);
        }

      public:

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

        ELEMENT const* lookup (char const* key) const {
          KeyValue l(key, strlen(key));
          KeyValue const& f = _array.findElement(l);

          if (f.key != 0 && strcmp(f.key, key) == 0) {
            return &f.value;
          }
          else {
            return 0;
          }
        }

      private:
        AssociativeArray<char const*, KeyValue, DictionaryDescription> _array;
    };
  }
}

#endif

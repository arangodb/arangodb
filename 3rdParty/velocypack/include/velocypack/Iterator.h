////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_ITERATOR_H
#define VELOCYPACK_ITERATOR_H 1

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"
#include "velocypack/Slice.h"
#include "velocypack/ValueType.h"

namespace arangodb {
  namespace velocypack {

    class ArrayIterator {

      public:

        ArrayIterator () = delete;

        ArrayIterator (Slice const& slice) 
          : _slice(slice), _size(_slice.length()), _position(0) {

          if (slice.type() != ValueType::Array) {
            throw Exception(Exception::InvalidValueType, "Expecting Array slice");
          }
        }
        
        ArrayIterator (ArrayIterator const& other) 
          : _slice(other._slice), _size(other._size), _position(0) {
        }
        
        ArrayIterator& operator= (ArrayIterator const& other) {
          _slice = other._slice;
          _size  = other._size;
          _position = 0;
          return *this;
        }

        Slice operator++ () { 
          return _slice.at(_position++);
        }

        bool operator!= (ArrayIterator const& other) const {
          return _position != other._position;
        }

        Slice operator* () const { 
          return _slice.at(_position); 
        }

        ArrayIterator begin () { 
          return ArrayIterator(_slice);
        }

        ArrayIterator begin () const { 
          return ArrayIterator(_slice);
        }

        ArrayIterator end () { 
          auto it = ArrayIterator(_slice);
          it._position = it._size;
          return it;
        }

        ArrayIterator end () const { 
          auto it = ArrayIterator(_slice);
          it._position = it._size;
          return it;
        }
        
        inline bool valid () const throw() {
          return (_position < _size);
        }

        inline Slice value () const {
          if (_position >= _size) {
            throw Exception(Exception::IndexOutOfBounds);
          }
          return _slice.at(_position);
        }

        inline bool next () throw() {
          ++_position;
          return valid();
        }

      private: 

        Slice _slice;
        ValueLength _size;
        ValueLength _position;
    };
    
    class ObjectIterator {

      public:

        struct ObjectPair {
          ObjectPair (Slice const& key, Slice const& value) 
            : key(key), value(value) {
          }
          Slice const key;
          Slice const value;
        };

        ObjectIterator () = delete;

        ObjectIterator (Slice const& slice) 
          : _slice(slice), _size(_slice.length()), _position(0) {

          if (slice.type() != ValueType::Object) {
            throw Exception(Exception::InvalidValueType, "Expecting Object slice");
          }
        }
        
        ObjectIterator (ObjectIterator const& other) 
          : _slice(other._slice), _size(other._size), _position(0) {
        }
        
        ObjectIterator& operator= (ObjectIterator const& other) {
          _slice = other._slice;
          _size  = other._size;
          _position = 0;
          return *this;
        }

        ObjectPair operator++ () { 
          ObjectPair current(_slice.keyAt(_position), _slice.valueAt(_position)); 
          ++_position;
          return current;
        }

        bool operator!= (ObjectIterator const& other) const {
          return _position != other._position;
        }

        ObjectPair operator* () const { 
          return ObjectPair(_slice.keyAt(_position), _slice.valueAt(_position)); 
        }

        ObjectIterator begin () { 
          return ObjectIterator(_slice);
        }

        ObjectIterator begin () const { 
          return ObjectIterator(_slice);
        }

        ObjectIterator end () { 
          auto it = ObjectIterator(_slice);
          it._position = it._size;
          return it;
        }

        ObjectIterator end () const { 
          auto it = ObjectIterator(_slice);
          it._position = it._size;
          return it;
        }

        inline bool valid () const throw() {
          return (_position < _size);
        }
        
        inline Slice key () const {
          if (_position >= _size) {
            throw Exception(Exception::IndexOutOfBounds);
          }
          return _slice.keyAt(_position);
        }

        inline Slice value () const {
          if (_position >= _size) {
            throw Exception(Exception::IndexOutOfBounds);
          }
          return _slice.valueAt(_position);
        }

        inline bool next () throw() {
          ++_position;
          return valid();
        }

      private: 

        Slice _slice;
        ValueLength _size;
        ValueLength _position;
    };

  }  // namespace arangodb::velocypack
}  // namespace arangodb

#endif

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

#ifndef VELOCYPACK_VALUE_H
#define VELOCYPACK_VALUE_H 1

#include <cstdint>
#include <string>

#include "velocypack/velocypack-common.h"
#include "velocypack/ValueType.h"

namespace arangodb {
  namespace velocypack {

    class Value {
      // Convenience class for more compact notation

      friend class Builder;

      public:

        enum CType {
          None     = 0,
          Bool     = 1,
          Double   = 2,
          Int64    = 3,
          UInt64   = 4,
          String   = 5,
          CharPtr  = 6,
          VoidPtr  = 7
        };
       
      private:

        ValueType _valueType;
        CType     _cType;    // denotes variant used, 0: none

        union {
          bool b;                // 1: bool
          double d;              // 2: double
          int64_t i;             // 3: int64_t
          uint64_t u;            // 4: uint64_t
          std::string const* s;  // 5: std::string
          char const* c;         // 6: char const*
          void const* e;         // external
        } _value;

      public:

        explicit Value (ValueType t = ValueType::Null) 
          : _valueType(t), _cType(CType::None) {
        }
        explicit Value (bool b, ValueType t = ValueType::Bool) 
          : _valueType(t), _cType(CType::Bool) {
          _value.b = b;
        }
        explicit Value (double d, ValueType t = ValueType::Double) 
          : _valueType(t), _cType(CType::Double) {
          _value.d = d;
        }
        explicit Value (void const* e, ValueType t = ValueType::External)
          : _valueType(t), _cType(CType::VoidPtr) {
          _value.e = e;
        }
        explicit Value (char const* c, ValueType t = ValueType::String)
          : _valueType(t), _cType(CType::CharPtr) {
          _value.c = c;
        }
        explicit Value (int32_t i, ValueType t = ValueType::Int)
          : _valueType(t), _cType(CType::Int64) {
          _value.i = static_cast<int64_t>(i);
        }
        explicit Value (uint32_t u, ValueType t = ValueType::UInt)
          : _valueType(t), _cType(CType::UInt64) {
          _value.u = static_cast<uint64_t>(u);
        }
        explicit Value (int64_t i, ValueType t = ValueType::Int)
          : _valueType(t), _cType(CType::Int64) {
          _value.i = i;
        }
        explicit Value (uint64_t u, ValueType t = ValueType::UInt)
          : _valueType(t), _cType(CType::UInt64) {
          _value.u = u;
        }
        explicit Value (std::string const& s, ValueType t = ValueType::String)
          : _valueType(t), _cType(CType::String) {
          _value.s = &s;
        }

        ValueType valueType () const {
          return _valueType;
        }

        CType cType () const {
          return _cType;
        }

        bool isString () const {
          return _valueType == ValueType::String;
        }

        bool getBool () const {
          VELOCYPACK_ASSERT(_cType == Bool);
          return _value.b;
        }

        double getDouble () const {
          VELOCYPACK_ASSERT(_cType == Double);
          return _value.d;
        }

        int64_t getInt64 () const {
          VELOCYPACK_ASSERT(_cType == Int64);
          return _value.i;
        }

        uint64_t getUInt64 () const {
          VELOCYPACK_ASSERT(_cType == UInt64);
          return _value.u;
        }

        std::string const* getString () const {
          VELOCYPACK_ASSERT(_cType == String);
          return _value.s;
        }

        void const* getExternal () const {
          VELOCYPACK_ASSERT(_cType == VoidPtr);
          return _value.e;
        }

        char const* getCharPtr () const {
          VELOCYPACK_ASSERT(_cType == CharPtr);
          return _value.c;
        }

    };

    class ValuePair {
        uint8_t const* _start;
        uint64_t       _size;
        ValueType      _type;

      public:

        explicit ValuePair (uint8_t const* start, uint64_t size,
                            ValueType type = ValueType::Binary)
          : _start(start), _size(size), _type(type) {
        }

        explicit ValuePair (char const* start, uint64_t size,
                            ValueType type = ValueType::Binary)
          : _start(reinterpret_cast<uint8_t const*>(start)),
            _size(size), _type(type) {
        }

        explicit ValuePair (uint64_t size,
                            ValueType type = ValueType::Binary)
          : _start(nullptr), _size(size), _type(type) {
        }

        uint8_t const* getStart () const {
          return _start;
        }

        uint64_t getSize () const {
          return _size;
        }

        ValueType valueType () const {
          return _type;
        }

        bool isString () const {
          return _type == ValueType::String;
        }
    };
       
  }  // namespace arangodb::velocypack
}  // namespace arangodb

#endif

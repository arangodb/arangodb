////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQL_VALUE_H
#define ARANGOD_AQL_AQL_VALUE_H 1

#include "Basics/Common.h"
#include "Aql/Range.h"
#include "Aql/types.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <v8.h>

namespace arangodb {
class AqlTransaction;

namespace aql {
class AqlItemBlock;

struct AqlValue final {
 friend struct std::hash<arangodb::aql::AqlValue>;
 friend struct std::equal_to<arangodb::aql::AqlValue>;

 public:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief AqlValueType, indicates what sort of value we have
  //////////////////////////////////////////////////////////////////////////////

  enum AqlValueType : uint8_t { 
    DOCUMENT, // contains a pointer to a document in the database
    VPACK_INLINE, // contains vpack, inline
    VPACK_EXTERNAL, // contains vpack, via pointer to a Buffer
    DOCVEC, // a vector of blocks of results coming from a subquery
    RANGE // a pointer to a range remembering lower and upper bound
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Holds the actual data for this AqlValue
  /// it has the following semantics:
  ///
  /// All values with a size less than 16 will be stored directly in this
  /// AqlValue using the data.internal structure.
  /// All values of a larger size will be stored in _data.external.
  /// The last byte of this union will be used to identify if we have to use
  /// internal or external.
  /// Delete of the Buffer should free every structure that is not using the
  /// VPack external or reference value type.
  //////////////////////////////////////////////////////////////////////////////

 private:
  union {
    uint8_t internal[16];
    uint8_t const* document;
    arangodb::velocypack::Buffer<uint8_t>* buffer;
    std::vector<AqlItemBlock*>* docvec;
    Range const* range;
  } _data;

 public:
  // construct an empty AqlValue
  AqlValue() {
    initFromSlice(arangodb::velocypack::Slice());
  }
  
  // construct from document
  explicit AqlValue(uint8_t const* document) {
    _data.document = document;
    setType(AqlValueType::DOCUMENT);
  }
  
  // construct from docvec, taking over its ownership
  explicit AqlValue(std::vector<AqlItemBlock*>* docvec) {
    _data.docvec = docvec;
    setType(AqlValueType::DOCVEC);
  }

  // construct boolean value type
  explicit AqlValue(bool value) {
    initFromSlice(value ? arangodb::basics::VelocyPackHelper::TrueValue() : arangodb::basics::VelocyPackHelper::FalseValue());
  }
  
  // construct from std::string
  explicit AqlValue(std::string const& value) {
    VPackBuilder builder;
    builder.add(VPackValue(value));
    initFromSlice(builder.slice());
  }
  
  // construct from Buffer, taking over its ownership
  explicit AqlValue(arangodb::velocypack::Buffer<uint8_t>* buffer) {
    _data.buffer = buffer;
    setType(AqlValueType::VPACK_EXTERNAL);
  }
  
  // construct from Builder
  explicit AqlValue(arangodb::velocypack::Builder const& builder) {
    TRI_ASSERT(builder.isClosed());
    initFromSlice(builder.slice());
  }
  
  explicit AqlValue(arangodb::velocypack::Builder const* builder) : AqlValue(*builder) {}

  // construct from Slice
  explicit AqlValue(arangodb::velocypack::Slice const& slice) {
    initFromSlice(slice);
  }
  
  // construct range type
  AqlValue(int64_t low, int64_t high) {
    _data.range = new Range(low, high);
    setType(AqlValueType::RANGE);
  }
  
  ~AqlValue() = default;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the value must be destroyed
  //////////////////////////////////////////////////////////////////////////////

  bool requiresDestruction() const {
    AqlValueType t = type();
    return (t == VPACK_EXTERNAL || t == DOCVEC || t == RANGE);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief hashes the value
  //////////////////////////////////////////////////////////////////////////////

  uint64_t hash(arangodb::AqlTransaction*) const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the value is empty / none
  //////////////////////////////////////////////////////////////////////////////
  
  bool isEmpty() const { 
    if (type() != VPACK_INLINE) {
      return false;
    }
    return arangodb::velocypack::Slice(_data.internal).isNone();
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the value is a range
  //////////////////////////////////////////////////////////////////////////////

  bool isRange() const {
    return type() == RANGE;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the value is a docvec
  //////////////////////////////////////////////////////////////////////////////
  
  bool isDocvec() const {
    return type() == DOCVEC;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the value contains a null value
  //////////////////////////////////////////////////////////////////////////////

  bool isNull(bool emptyIsNull) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the value is a number
  //////////////////////////////////////////////////////////////////////////////

  bool isNumber() const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the value is a string
  //////////////////////////////////////////////////////////////////////////////
  
  bool isString() const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the value is an object
  //////////////////////////////////////////////////////////////////////////////

  bool isObject() const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the value is an array (note: this treats ranges
  /// as arrays, too!)
  //////////////////////////////////////////////////////////////////////////////

  bool isArray() const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the (array) length (note: this treats ranges as arrays, too!)
  //////////////////////////////////////////////////////////////////////////////

  size_t length() const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the (array) element at position 
  //////////////////////////////////////////////////////////////////////////////

  AqlValue at(int64_t position, bool& mustDestroy, bool copy) const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the (object) element by name(s)
  //////////////////////////////////////////////////////////////////////////////
  
  AqlValue get(arangodb::AqlTransaction* trx,
               std::string const& name, bool& mustDestroy, bool copy) const;
  AqlValue get(arangodb::AqlTransaction* trx,
               std::vector<char const*> const& names, bool& mustDestroy,
               bool copy) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the numeric value of an AqlValue
  //////////////////////////////////////////////////////////////////////////////

  double toDouble() const;
  double toDouble(bool& failed) const;
  int64_t toInt64() const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not an AqlValue evaluates to true/false
  //////////////////////////////////////////////////////////////////////////////

  bool toBoolean() const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the range value
  //////////////////////////////////////////////////////////////////////////////

  Range const* range() const {
    TRI_ASSERT(isRange());
    return _data.range;
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the total size of the docvecs
  //////////////////////////////////////////////////////////////////////////////

  size_t docvecSize() const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the item block at position
  //////////////////////////////////////////////////////////////////////////////
  
  AqlItemBlock* docvecAt(size_t position) const {
    TRI_ASSERT(isDocvec());
    return _data.docvec->at(position);
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief construct a V8 value as input for the expression execution in V8
  /// only construct those attributes that are needed in the expression
  //////////////////////////////////////////////////////////////////////////////

  v8::Handle<v8::Value> toV8Partial(v8::Isolate* isolate,
                                    arangodb::AqlTransaction*,
                                    std::unordered_set<std::string> const&) const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief construct a V8 value as input for the expression execution in V8
  //////////////////////////////////////////////////////////////////////////////

  v8::Handle<v8::Value> toV8(v8::Isolate* isolate, arangodb::AqlTransaction*) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief materializes a value into the builder
  //////////////////////////////////////////////////////////////////////////////

  void toVelocyPack(arangodb::AqlTransaction*, arangodb::velocypack::Builder& builder) const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief materialize a value into a new one. this expands docvecs and 
  /// ranges
  //////////////////////////////////////////////////////////////////////////////
  
  AqlValue materialize(arangodb::AqlTransaction*, bool& hasCopied) const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief clone a value
  //////////////////////////////////////////////////////////////////////////////

  AqlValue clone() const;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief invalidates/resets a value to None, not freeing any memory
  //////////////////////////////////////////////////////////////////////////////

  void erase() {
    initFromSlice(arangodb::velocypack::Slice());
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroy, explicit destruction, only when needed
  //////////////////////////////////////////////////////////////////////////////
  
  void destroy();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create an AqlValue from a vector of AqlItemBlock*s
  //////////////////////////////////////////////////////////////////////////////

  static AqlValue CreateFromBlocks(arangodb::AqlTransaction*,
                                    std::vector<AqlItemBlock*> const&,
                                    std::vector<std::string> const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create an AqlValue from a vector of AqlItemBlock*s
  //////////////////////////////////////////////////////////////////////////////

  static AqlValue CreateFromBlocks(arangodb::AqlTransaction*,
                                    std::vector<AqlItemBlock*> const&,
                                    arangodb::aql::RegisterId);
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief compare function for two values
  /// TODO: implement
  //////////////////////////////////////////////////////////////////////////////

  static int Compare(arangodb::AqlTransaction*, 
                     AqlValue const& left, AqlValue const& right, bool useUtf8);

 private:
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the type of this value. If true it uses an external pointer
  /// if false it uses the internal data structure
  //////////////////////////////////////////////////////////////////////////////

  AqlValueType type() const {
    return static_cast<AqlValueType>(_data.internal[sizeof(_data.internal) - 1]);
  }
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief initializes value from a slice
  //////////////////////////////////////////////////////////////////////////////
  
  void initFromSlice(arangodb::velocypack::Slice const& slice) {
    arangodb::velocypack::ValueLength length = slice.byteSize();
    if (length < sizeof(_data.internal)) {
      // Use internal
      memcpy(_data.internal, slice.begin(), length);
      setType(AqlValueType::VPACK_INLINE);
    } else {
      // Use external
      _data.buffer = new arangodb::velocypack::Buffer<uint8_t>(length);
      _data.buffer->append(reinterpret_cast<char const*>(slice.begin()), length);
      setType(AqlValueType::VPACK_EXTERNAL);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the slice from the value
  //////////////////////////////////////////////////////////////////////////////

 public:
 // TODO: make this private again
  arangodb::velocypack::Slice slice() const;
 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the value type
  //////////////////////////////////////////////////////////////////////////////

  void setType(AqlValueType type) {
    _data.internal[sizeof(_data.internal) - 1] = type;
  }
};

class AqlValueGuard {
 public:
  AqlValueGuard() = delete;
  AqlValueGuard(AqlValueGuard const&) = delete;
  AqlValueGuard& operator=(AqlValueGuard const&) = delete;

  AqlValueGuard(AqlValue& value, bool destroy) : value(value), destroy(destroy) {}
  ~AqlValueGuard() { 
    if (destroy) { 
      value.destroy();
    } 
  }
  void steal() { destroy = false; }
 private:
  AqlValue& value;
  bool destroy;
};

struct AqlValueMaterializer {
  explicit AqlValueMaterializer(arangodb::AqlTransaction* trx) 
      : trx(trx), materialized(), hasCopied(false) {}

  ~AqlValueMaterializer() { 
    if (hasCopied) { 
      materialized.destroy(); 
    } 
  }

  arangodb::velocypack::Slice slice(AqlValue const& value) {
    materialized = value.materialize(trx, hasCopied);
    return materialized.slice();
  }

  arangodb::AqlTransaction* trx;
  AqlValue materialized;
  bool hasCopied;
};

static_assert(sizeof(AqlValue) == 16, "invalid AqlValue size");

}  // closes namespace arangodb::aql
}  // closes namespace arangodb

////////////////////////////////////////////////////////////////////////////////
/// @brief hash function for AqlValue objects
////////////////////////////////////////////////////////////////////////////////

namespace std {

template <>
struct hash<arangodb::aql::AqlValue> {
  size_t operator()(arangodb::aql::AqlValue const& x) const {
    std::hash<uint8_t> intHash;
    std::hash<void const*> ptrHash;
    arangodb::aql::AqlValue::AqlValueType type = x.type();
    size_t res = intHash(type);
    switch (type) {
      case arangodb::aql::AqlValue::DOCUMENT: {
        return res ^ ptrHash(x._data.document);
      }
      case arangodb::aql::AqlValue::VPACK_INLINE: {
        return res ^ static_cast<size_t>(arangodb::velocypack::Slice(&x._data.internal[0]).hash());
      }
      case arangodb::aql::AqlValue::VPACK_EXTERNAL: {
        return res ^ ptrHash(x._data.buffer);
      }
      case arangodb::aql::AqlValue::DOCVEC: {
        return res ^ ptrHash(x._data.docvec);
      }
      case arangodb::aql::AqlValue::RANGE: {
        return res ^ ptrHash(x._data.range);
      }
    }

    TRI_ASSERT(false);
    return 0;
  }
};

template <>
struct equal_to<arangodb::aql::AqlValue> {
  bool operator()(arangodb::aql::AqlValue const& a,
                  arangodb::aql::AqlValue const& b) const {
    arangodb::aql::AqlValue::AqlValueType type = a.type();
    if (type != b.type()) {
      return false;
    }
    switch (type) {
      case arangodb::aql::AqlValue::DOCUMENT: {
        return a._data.document == b._data.document;
      }
      case arangodb::aql::AqlValue::VPACK_INLINE: {
        return arangodb::velocypack::Slice(&a._data.internal[0]).equals(arangodb::velocypack::Slice(&b._data.internal[0]));
      }
      case arangodb::aql::AqlValue::VPACK_EXTERNAL: {
        return a._data.buffer == b._data.buffer;
      }
      case arangodb::aql::AqlValue::DOCVEC: {
        return a._data.docvec == b._data.docvec;
      }
      case arangodb::aql::AqlValue::RANGE: {
        return a._data.range == b._data.range;
      }
    }
    
    TRI_ASSERT(false);
    return false;
  }
};

}  // closes namespace std

#endif

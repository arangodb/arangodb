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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQL_VALUE_H
#define ARANGOD_AQL_AQL_VALUE_H 1

#include "Basics/Common.h"
#include "Aql/Range.h"
#include "Aql/types.h"
#include "Basics/ConditionalDeleter.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <v8.h>

// some functionality borrowed from 3rdParty/velocypack/include/velocypack
// this is a copy of that functionality, because the functions in velocypack
// are not accessible from here
namespace {
  static inline uint64_t toUInt64(int64_t v) noexcept {
    // If v is negative, we need to add 2^63 to make it positive,
    // before we can cast it to an uint64_t:
    uint64_t shift2 = 1ULL << 63;
    int64_t shift = static_cast<int64_t>(shift2 - 1);
    return v >= 0 ? static_cast<uint64_t>(v)
                  : static_cast<uint64_t>((v + shift) + 1) + shift2;
    // Note that g++ and clang++ with -O3 compile this away to
    // nothing. Further note that a plain cast from int64_t to
    // uint64_t is not guaranteed to work for negative values!
  }

  // returns number of bytes required to store the value in 2s-complement
  static inline uint8_t intLength(int64_t value) noexcept {
    if (value >= -0x80 && value <= 0x7f) {
      // shortcut for the common case
      return 1;
    }
    uint64_t x = value >= 0 ? static_cast<uint64_t>(value)
                            : static_cast<uint64_t>(-(value + 1));
    uint8_t xSize = 0;
    do {
      xSize++;
      x >>= 8;
    } while (x >= 0x80);
    return xSize + 1;
  }
}

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
class AqlItemBlock;

// no-op struct used only internally to indicate that we want
// to copy the data behind the passed pointer
struct AqlValueHintCopy {
  explicit AqlValueHintCopy(uint8_t const* ptr) : ptr(ptr) {}
  uint8_t const* ptr;
};

// no-op struct used only internally to indicate that we want
// to NOT copy the data behind the passed pointer
struct AqlValueHintNoCopy {
  explicit AqlValueHintNoCopy(uint8_t const* ptr) : ptr(ptr) {}
  uint8_t const* ptr;
};

// no-op struct used only internally to indicate that we want
// to pass the ownership of the data behind the passed pointer
// to the callee
struct AqlValueHintTransferOwnership {
  explicit AqlValueHintTransferOwnership(uint8_t* ptr) : ptr(ptr) {}
  uint8_t* ptr;
};

struct AqlValue final {
 friend struct std::hash<arangodb::aql::AqlValue>;
 friend struct std::equal_to<arangodb::aql::AqlValue>;
 static std::array<std::string const, 8> typeStrings;

 public:

  /// @brief AqlValueType, indicates what sort of value we have
  enum AqlValueType : uint8_t { 
    VPACK_SLICE_POINTER, // contains a pointer to a vpack document, memory is not managed!
    VPACK_INLINE, // contains vpack data, inline
    VPACK_MANAGED_SLICE, // contains vpack, via pointer to a managed uint8_t slice
    VPACK_MANAGED_BUFFER, // contains vpack, via pointer to a managed buffer
    DOCVEC, // a vector of blocks of results coming from a subquery, managed
    RANGE // a pointer to a range remembering lower and upper bound, managed
  };

  /// @brief Holds the actual data for this AqlValue
  /// The last byte of this union (_data.internal[15]) will be used to identify 
  /// the type of the contained data:
  ///
  /// VPACK_SLICE_POINTER: data may be referenced via a pointer to a VPack slice
  /// existing somewhere in memory. The AqlValue is not responsible for managing
  /// this memory.
  /// VPACK_INLINE: VPack values with a size less than 16 bytes can be stored 
  /// directly inside the data.internal structure. All data is stored inline, 
  /// so there is no need for memory management.
  /// VPACK_MANAGED_SLICE: all values of a larger size will be stored in 
  /// _data.slice via a managed uint8_t* object. The uint8_t* points to a VPack
  /// data and is managed by the AqlValue.
  /// VPACK_MANAGED_BUFFER: all values of a larger size will be stored in 
  /// _data.external via a managed VPackBuffer object. The Buffer is managed
  /// by the AqlValue.
  /// DOCVEC: a managed vector of AqlItemBlocks, for storing subquery results.
  /// The vector and ItemBlocks are managed by the AqlValue
  /// RANGE: a managed range object. The memory is managed by the AqlValue
 private:
  union {
    uint8_t internal[16];
    uint8_t const* pointer;
    uint8_t* slice;
    arangodb::velocypack::Buffer<uint8_t>* buffer;
    std::vector<AqlItemBlock*>* docvec;
    Range const* range;
  } _data;

 public:
  // construct an empty AqlValue
  // note: this is the default constructor and should be as cheap as possible
  AqlValue() noexcept {
    // construct a slice of type None
    _data.internal[0] = '\x00';
    setType(AqlValueType::VPACK_INLINE);
  }
  
  // construct from pointer, not copying!
  explicit AqlValue(uint8_t const* pointer) {
    // we must get rid of Externals first here, because all
    // methods that use VPACK_SLICE_POINTER expect its contents
    // to be non-Externals
    if (*pointer == '\x1d') {
      // an external
      setPointer<false>(VPackSlice(pointer).resolveExternals().begin());
    } else {
      setPointer<false>(pointer);
    }
    TRI_ASSERT(!VPackSlice(_data.pointer).isExternal());
  }
  
  // construct from docvec, taking over its ownership
  explicit AqlValue(std::vector<AqlItemBlock*>* docvec) noexcept {
    TRI_ASSERT(docvec != nullptr);
    _data.docvec = docvec;
    setType(AqlValueType::DOCVEC);
  }

  // construct boolean value type
  explicit AqlValue(bool value) {
    VPackSlice slice(value ? arangodb::basics::VelocyPackHelper::TrueValue() : arangodb::basics::VelocyPackHelper::FalseValue());
    memcpy(_data.internal, slice.begin(), static_cast<size_t>(slice.byteSize()));
    setType(AqlValueType::VPACK_INLINE);
  }

  // construct from a double value
  explicit AqlValue(double value) {
    if (std::isnan(value) || !std::isfinite(value) || value == HUGE_VAL || value == -HUGE_VAL) {
      // null
      _data.internal[0] = 0x18;
    } else {
      // a "real" double
      _data.internal[0] = 0x1b;
      uint64_t dv;
      memcpy(&dv, &value, sizeof(double));
      VPackValueLength vSize = sizeof(double);
      int i = 1;
      for (uint64_t x = dv; vSize > 0; vSize--) {
        _data.internal[i] = x & 0xff;
        x >>= 8;
        ++i;
      }
    }
    setType(AqlValueType::VPACK_INLINE);
  }
  
  // construct from an int64 value
  explicit AqlValue(int64_t value) noexcept {
    if (value >= 0 && value <= 9) {
      // a smallint
      _data.internal[0] = static_cast<uint8_t>(0x30U + value);
    } else if (value < 0 && value >= -6) {
      // a negative smallint
      _data.internal[0] = static_cast<uint8_t>(0x40U + value);
    } else {
      uint8_t vSize = intLength(value);
      uint64_t x;
      if (vSize == 8) {
        x = toUInt64(value);
      } else {
        int64_t shift = 1LL << (vSize * 8 - 1);  // will never overflow!
        x = value >= 0 ? static_cast<uint64_t>(value)
                       : static_cast<uint64_t>(value + shift) + shift;
      }
      _data.internal[0] = 0x1fU + vSize;
      int i = 1;
      while (vSize-- > 0) {
        _data.internal[i] = x & 0xffU;
        ++i;
        x >>= 8;
      }
    }
    setType(AqlValueType::VPACK_INLINE);
  }
  
  // construct from a uint64 value
  explicit AqlValue(uint64_t value) noexcept {
    if (value <= 9) {
      // a smallint
      _data.internal[0] = static_cast<uint8_t>(0x30U + value);
    } else {
      int i = 1;
      uint8_t vSize = 0;
      do {
        vSize++;
        _data.internal[i] = static_cast<uint8_t>(value & 0xffU);
        ++i;
        value >>= 8;
      } while (value != 0);
      _data.internal[0] = 0x27U + vSize;
    }
    setType(AqlValueType::VPACK_INLINE);
  }
  
  // construct from char* and length, copying the string
  AqlValue(char const* value, size_t length) {
    TRI_ASSERT(value != nullptr);
    if (length == 0) {
      // empty string
      _data.internal[0] = 0x40;
      setType(AqlValueType::VPACK_INLINE);
      return;
    }
    if (length < sizeof(_data.internal) - 1) {
      // short string... can store it inline
      _data.internal[0] = static_cast<uint8_t>(0x40 + length);
      memcpy(_data.internal + 1, value, length);
      setType(AqlValueType::VPACK_INLINE);
    } else if (length <= 126) {
      // short string... cannot store inline, but we don't need to
      // create a full-featured Builder object here
      _data.slice = new uint8_t[length + 1];
      _data.slice[0] = static_cast<uint8_t>(0x40U + length);
      memcpy(&_data.slice[1], value, length);
      setType(AqlValueType::VPACK_MANAGED_SLICE);
    } else {
      // long string
      // create a big enough uint8_t buffer
      _data.slice = new uint8_t[length + 9];
      _data.slice[0] = static_cast<uint8_t>(0xbfU);
      uint64_t v = length;
      for (uint64_t i = 0; i < 8; ++i) {
        _data.slice[i + 1] = v & 0xffU;
        v >>= 8;
      }
      memcpy(&_data.slice[9], value, length);
      setType(AqlValueType::VPACK_MANAGED_SLICE);
    }
  }
  
  // construct from std::string
  explicit AqlValue(std::string const& value) : AqlValue(value.c_str(), value.size()) {}
  
  // construct from Buffer, potentially taking over its ownership
  // (by adjusting the boolean passed)
  AqlValue(arangodb::velocypack::Buffer<uint8_t>* buffer, bool& shouldDelete) {
    TRI_ASSERT(buffer != nullptr);
    TRI_ASSERT(shouldDelete); // here, the Buffer is still owned by the caller

    // intentionally do not resolve externals here
    // if (slice.isExternal()) {
    //   // recursively resolve externals
    //   slice = slice.resolveExternals();
    // }
    if (buffer->length() < sizeof(_data.internal)) {
      // Use inline value
      memcpy(_data.internal, buffer->data(), static_cast<size_t>(buffer->length()));
      setType(AqlValueType::VPACK_INLINE);
    } else {
      // Use managed buffer, simply reuse the pointer and adjust the original
      // Buffer's deleter
      _data.buffer = buffer;
      setType(AqlValueType::VPACK_MANAGED_BUFFER);
      shouldDelete = false; // adjust deletion control variable
    }
  }
  
  // construct from pointer, not copying!
  explicit AqlValue(AqlValueHintNoCopy const& ptr) noexcept {
    setPointer<true>(ptr.ptr);
    TRI_ASSERT(!VPackSlice(_data.pointer).isExternal());
  }
  
  // construct from pointer, copying the data behind the pointer
  explicit AqlValue(AqlValueHintCopy const& ptr) {
    TRI_ASSERT(ptr.ptr != nullptr);
    initFromSlice(VPackSlice(ptr.ptr));
  }
  
  // construct from pointer, taking over the ownership
  explicit AqlValue(AqlValueHintTransferOwnership const& ptr) {
    TRI_ASSERT(ptr.ptr != nullptr);
    _data.slice = ptr.ptr;
    setType(AqlValueType::VPACK_MANAGED_SLICE);
  }
  
  // construct from Builder, copying contents
  explicit AqlValue(arangodb::velocypack::Builder const& builder) {
    TRI_ASSERT(builder.isClosed());
    initFromSlice(builder.slice());
  }

  // construct from Builder, copying contents
  explicit AqlValue(arangodb::velocypack::Builder const* builder) {
    TRI_ASSERT(builder->isClosed());
    initFromSlice(builder->slice());
  }
  
  // construct from Slice, copying contents
  explicit AqlValue(arangodb::velocypack::Slice const& slice) {
    initFromSlice(slice);
  }
  
  // construct range type
  AqlValue(int64_t low, int64_t high) {
    _data.range = new Range(low, high);
    setType(AqlValueType::RANGE);
  }
 
  /// @brief AqlValues can be copied and moved as required
  /// memory management is not performed via AqlValue destructor but via 
  /// explicit calls to destroy()
  AqlValue(AqlValue const&) noexcept = default; 
  AqlValue& operator=(AqlValue const&) noexcept = default; 
  AqlValue(AqlValue&&) noexcept = default; 
  AqlValue& operator=(AqlValue&&) noexcept = default; 

  ~AqlValue() = default;
  
  /// @brief whether or not the value must be destroyed
  inline bool requiresDestruction() const noexcept {
    auto t = type();
    return (t != VPACK_SLICE_POINTER && t != VPACK_INLINE);
  }

  /// @brief whether or not the value is empty / none
  inline bool isEmpty() const noexcept { 
    return (_data.internal[0] == '\x00' && _data.internal[sizeof(_data.internal) - 1] == VPACK_INLINE);
  }
  
  /// @brief whether or not the value is a pointer
  inline bool isPointer() const noexcept {
    return type() == VPACK_SLICE_POINTER;
  }

  /// @brief whether or not the value is an external manager document
  inline bool isManagedDocument() const noexcept {
    return isPointer() && (_data.internal[sizeof(_data.internal) - 2] == 1);
  }
  
  /// @brief whether or not the value is a range
  inline bool isRange() const noexcept {
    return type() == RANGE;
  }
  
  /// @brief whether or not the value is a docvec
  inline bool isDocvec() const noexcept {
    return type() == DOCVEC;
  }

  /// @brief hashes the value
  uint64_t hash(transaction::Methods*, uint64_t seed = 0xdeadbeef) const;

  /// @brief whether or not the value contains a none value
  bool isNone() const noexcept;
  
  /// @brief whether or not the value contains a null value
  bool isNull(bool emptyIsNull) const noexcept;

  /// @brief whether or not the value contains a boolean value
  bool isBoolean() const noexcept;

  /// @brief whether or not the value is a number
  bool isNumber() const noexcept;
  
  /// @brief whether or not the value is a string
  bool isString() const noexcept;
  
  /// @brief whether or not the value is an object
  bool isObject() const noexcept;
  
  /// @brief whether or not the value is an array (note: this treats ranges
  /// as arrays, too!)
  bool isArray() const noexcept;

  // @brief return a string describing the content of this aqlvalue
  std::string const & getTypeString() const noexcept;

  
  /// @brief get the (array) length (note: this treats ranges as arrays, too!)
  size_t length() const;
  
  /// @brief get the (array) element at position 
  AqlValue at(transaction::Methods* trx, int64_t position, bool& mustDestroy, bool copy) const;
  
  /// @brief get the _key attribute from an object/document
  AqlValue getKeyAttribute(transaction::Methods* trx,
                           bool& mustDestroy, bool copy) const;
  /// @brief get the _id attribute from an object/document
  AqlValue getIdAttribute(transaction::Methods* trx,
                          bool& mustDestroy, bool copy) const;
  /// @brief get the _from attribute from an object/document
  AqlValue getFromAttribute(transaction::Methods* trx,
                            bool& mustDestroy, bool copy) const;
  /// @brief get the _to attribute from an object/document
  AqlValue getToAttribute(transaction::Methods* trx,
                          bool& mustDestroy, bool copy) const;
  
  /// @brief get the (object) element by name(s)
  AqlValue get(transaction::Methods* trx,
               std::string const& name, bool& mustDestroy, bool copy) const;
  AqlValue get(transaction::Methods* trx,
               std::vector<std::string> const& names, bool& mustDestroy,
               bool copy) const;
  bool hasKey(transaction::Methods* trx, std::string const& name) const;

  /// @brief get the numeric value of an AqlValue
  double toDouble(transaction::Methods* trx) const;
  double toDouble(transaction::Methods* trx, bool& failed) const;
  int64_t toInt64(transaction::Methods* trx) const;
  
  /// @brief whether or not an AqlValue evaluates to true/false
  bool toBoolean() const;
  
  /// @brief return the range value
  Range const* range() const {
    TRI_ASSERT(isRange());
    return _data.range;
  }
  
  /// @brief return the total size of the docvecs
  size_t docvecSize() const;
  
  /// @brief return the item block at position
  AqlItemBlock* docvecAt(size_t position) const {
    TRI_ASSERT(isDocvec());
    return _data.docvec->at(position);
  }
  
  /// @brief construct a V8 value as input for the expression execution in V8
  /// only construct those attributes that are needed in the expression
  v8::Handle<v8::Value> toV8Partial(v8::Isolate* isolate,
                                    transaction::Methods*,
                                    std::unordered_set<std::string> const&) const;
  
  /// @brief construct a V8 value as input for the expression execution in V8
  v8::Handle<v8::Value> toV8(v8::Isolate* isolate, transaction::Methods*) const;

  /// @brief materializes a value into the builder
  void toVelocyPack(transaction::Methods*,
                    arangodb::velocypack::Builder& builder,
                    bool resolveExternals) const;

  /// @brief materialize a value into a new one. this expands docvecs and 
  /// ranges
  AqlValue materialize(transaction::Methods*, bool& hasCopied,
                       bool resolveExternals) const;

  /// @brief return the slice for the value
  /// this will throw if the value type is not VPACK_SLICE_POINTER, VPACK_INLINE,
  /// VPACK_MANAGED_SLICE or VPACK_MANAGED_BUFFER
  arangodb::velocypack::Slice slice() const;
  
  /// @brief clone a value
  AqlValue clone() const;
  
  /// @brief invalidates/resets a value to None, not freeing any memory
  void erase() noexcept {
    _data.internal[0] = '\x00';
    setType(AqlValueType::VPACK_INLINE);
  }
  
  /// @brief destroy, explicit destruction, only when needed
  void destroy();
  
  /// @brief returns the size of the dynamic memory allocated for the value
  size_t memoryUsage() const noexcept {
    auto const t = type();
    switch (t) {
      case VPACK_SLICE_POINTER:
      case VPACK_INLINE:
        return 0;
      case VPACK_MANAGED_SLICE:
        try {
          return VPackSlice(_data.slice).byteSize();
        } catch (...) {
          return 0;
        }
      case VPACK_MANAGED_BUFFER:
        return _data.buffer->size();
      case DOCVEC:
        // no need to count the memory usage for the item blocks in docvec.
        // these have already been counted elsewhere (in ctors of AqlItemBlock
        // and AqlItemBlock::setValue)
        return sizeof(AqlItemBlock*) * _data.docvec->size();
      case RANGE:
        return sizeof(Range);
    }
    return 0;
  }

  /// @brief create an AqlValue from a vector of AqlItemBlock*s
  static AqlValue CreateFromBlocks(transaction::Methods*,
                                    std::vector<AqlItemBlock*> const&,
                                    std::vector<std::string> const&);

  /// @brief create an AqlValue from a vector of AqlItemBlock*s
  static AqlValue CreateFromBlocks(transaction::Methods*,
                                    std::vector<AqlItemBlock*> const&,
                                    arangodb::aql::RegisterId);
  
  /// @brief compare function for two values
  static int Compare(transaction::Methods*, 
                     AqlValue const& left, AqlValue const& right, bool useUtf8);

 private:
  
  /// @brief Returns the type of this value. If true it uses an external pointer
  /// if false it uses the internal data structure
  inline AqlValueType type() const noexcept {
    return static_cast<AqlValueType>(_data.internal[sizeof(_data.internal) - 1]);
  }
  
  /// @brief initializes value from a slice
  void initFromSlice(arangodb::velocypack::Slice const& slice) {
    // intentionally do not resolve externals here
    // if (slice.isExternal()) {
    //   // recursively resolve externals
    //   slice = slice.resolveExternals();
    // }
    arangodb::velocypack::ValueLength length = slice.byteSize();
    if (length < sizeof(_data.internal)) {
      // Use inline value
      memcpy(_data.internal, slice.begin(), static_cast<size_t>(length));
      setType(AqlValueType::VPACK_INLINE);
    } else {
      // Use managed slice
      _data.slice = new uint8_t[length];
      memcpy(&_data.slice[0], slice.begin(), length);
      setType(AqlValueType::VPACK_MANAGED_SLICE);
    }
  }
  
  /// @brief sets the value type
  inline void setType(AqlValueType type) noexcept {
    _data.internal[sizeof(_data.internal) - 1] = type;
  }

  template<bool isManagedDocument>
  inline void setPointer(uint8_t const* pointer) noexcept {
    _data.pointer = pointer;
    // we use the byte at (size - 2) to distinguish between data pointing to database
    // documents (size[-2] == 1) and other data(size[-2] == 0)
    _data.internal[sizeof(_data.internal) - 2] = isManagedDocument ? 1 : 0;
    _data.internal[sizeof(_data.internal) - 1] = AqlValueType::VPACK_SLICE_POINTER;
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
  explicit AqlValueMaterializer(transaction::Methods* trx) 
      : trx(trx), materialized(), hasCopied(false) {}

  AqlValueMaterializer(AqlValueMaterializer const& other) 
      : trx(other.trx), materialized(other.materialized), hasCopied(other.hasCopied) {
    if (other.hasCopied) {
      // copy other's slice
      materialized = other.materialized.clone();
    }
  }
  
  AqlValueMaterializer& operator=(AqlValueMaterializer const& other) {
    if (this != &other) {
      TRI_ASSERT(trx == other.trx); // must be from same transaction
      if (hasCopied) {
        // destroy our own slice
        materialized.destroy();
        hasCopied = false;
      }
      // copy other's slice
      materialized = other.materialized.clone();
      hasCopied = other.hasCopied;
    }
    return *this;
  }
  
  AqlValueMaterializer(AqlValueMaterializer&& other) noexcept 
      : trx(other.trx), materialized(other.materialized), hasCopied(other.hasCopied) {
    // reset other
    other.hasCopied = false;
    // cppcheck-suppress *
    other.materialized = AqlValue();
  }
  
  AqlValueMaterializer& operator=(AqlValueMaterializer&& other) noexcept {
    if (this != &other) {
      TRI_ASSERT(trx == other.trx); // must be from same transaction
      if (hasCopied) {
        // destroy our own slice
        materialized.destroy();
      }
      // reset other
      materialized = other.materialized;
      hasCopied = other.hasCopied;
      other.materialized = AqlValue();
    }
    return *this;
  }

  ~AqlValueMaterializer() { 
    if (hasCopied) { 
      materialized.destroy(); 
    } 
  }

  arangodb::velocypack::Slice slice(AqlValue const& value,
                                    bool resolveExternals) {
    materialized = value.materialize(trx, hasCopied, resolveExternals);
    return materialized.slice();
  }

  transaction::Methods* trx;
  AqlValue materialized;
  bool hasCopied;
};

static_assert(sizeof(AqlValue) == 16, "invalid AqlValue size");

}  // closes namespace arangodb::aql
}  // closes namespace arangodb

/// @brief hash function for AqlValue objects
namespace std {

template <>
struct hash<arangodb::aql::AqlValue> {
  size_t operator()(arangodb::aql::AqlValue const& x) const {
    std::hash<uint8_t> intHash;
    std::hash<void const*> ptrHash;
    arangodb::aql::AqlValue::AqlValueType type = x.type();
    size_t res = intHash(type);
    switch (type) {
      case arangodb::aql::AqlValue::VPACK_SLICE_POINTER: { 
        return res ^ ptrHash(x._data.pointer);
      }
      case arangodb::aql::AqlValue::VPACK_INLINE: {
        return res ^ static_cast<size_t>(arangodb::velocypack::Slice(&x._data.internal[0]).hash());
      }
      case arangodb::aql::AqlValue::VPACK_MANAGED_SLICE: {
        return res ^ ptrHash(x._data.slice);
      }
      case arangodb::aql::AqlValue::VPACK_MANAGED_BUFFER: {
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
      case arangodb::aql::AqlValue::VPACK_SLICE_POINTER: {
        return a._data.pointer == b._data.pointer;
      }
      case arangodb::aql::AqlValue::VPACK_INLINE: {
        return arangodb::velocypack::Slice(&a._data.internal[0]).equals(arangodb::velocypack::Slice(&b._data.internal[0]));
      }
      case arangodb::aql::AqlValue::VPACK_MANAGED_SLICE: {
        return a._data.slice == b._data.slice;
      }
      case arangodb::aql::AqlValue::VPACK_MANAGED_BUFFER: {
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

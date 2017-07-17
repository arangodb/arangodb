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

#ifndef VELOCYPACK_SLICE_H
#define VELOCYPACK_SLICE_H 1

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <iosfwd>
#include <algorithm>
#include <functional>
#include <type_traits>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"
#include "velocypack/Options.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"

#ifndef VELOCYPACK_XXHASH
#ifndef VELOCYPACK_FASTHASH
#define VELOCYPACK_XXHASH
#endif
#endif

#ifdef VELOCYPACK_XXHASH
// forward for XXH64 function declared elsewhere
extern "C" unsigned long long XXH64(void const*, size_t, unsigned long long);

#define VELOCYPACK_HASH(mem, size, seed) XXH64(mem, size, seed)
#endif

#ifdef VELOCYPACK_FASTHASH
// forward for fasthash64 function declared elsewhere
uint64_t fasthash64(void const*, size_t, uint64_t);

#define VELOCYPACK_HASH(mem, size, seed) fasthash64(mem, size, seed)
#endif

namespace arangodb {
namespace velocypack {

class SliceScope;

class SliceStaticData {
  friend class Slice;
  static ValueLength const FixedTypeLengths[256];
  static ValueType const TypeMap[256];
  static unsigned int const WidthMap[32];
  static unsigned int const FirstSubMap[32];
};

class Slice {
  // This class provides read only access to a VPack value, it is
  // intentionally light-weight (only one pointer value), such that
  // it can easily be used to traverse larger VPack values.

  friend class Builder;
  friend class ArrayIterator;
  friend class ObjectIterator;

  uint8_t const* _start;

 public:

  // constructor for an empty Value of type None
  constexpr Slice() noexcept : Slice("\x00") {}

  // creates a slice of type None
  static constexpr Slice noneSlice() noexcept { return Slice("\x00"); }
  
  // creates a slice of type Illegal
  static constexpr Slice illegalSlice() noexcept { return Slice("\x17"); }

  // creates a slice of type Null
  static constexpr Slice nullSlice() noexcept { return Slice("\x18"); }
  
  // creates a slice of type Boolean with false value
  static constexpr Slice falseSlice() noexcept { return Slice("\x19"); }

  // creates a slice of type Boolean with true value
  static constexpr Slice trueSlice() noexcept { return Slice("\x1a"); }
  
  // creates a slice of type Smallint(0)
  static constexpr Slice zeroSlice() noexcept { return Slice("\x30"); }
  
  // creates a slice of type Array, empty
  static constexpr Slice emptyArraySlice() noexcept { return Slice("\x01"); }
  
  // creates a slice of type Object, empty
  static constexpr Slice emptyObjectSlice() noexcept { return Slice("\x0a"); }
  
  // creates a slice of type MinKey
  static constexpr Slice minKeySlice() noexcept { return Slice("\x1e"); }

  // creates a slice of type MaxKey
  static constexpr Slice maxKeySlice() noexcept { return Slice("\x1f"); }

  // creates a Slice from a pointer to a uint8_t array
  explicit constexpr Slice(uint8_t const* start) noexcept
      : _start(start) {}

  // creates a Slice from a pointer to a char array
  explicit constexpr Slice(char const* start) noexcept
    : _start((uint8_t const*)(start)) {} // reinterpret_cast does not work C++ 11 5.19.2
  
  // creates a Slice from Json and adds it to a scope
  static Slice fromJson(SliceScope& scope, std::string const& json,
                        Options const* options = &Options::Defaults);
  
  uint8_t const* begin() noexcept { return _start; }

  uint8_t const* begin() const noexcept { return _start; }

  uint8_t const* end() { return _start + byteSize(); }

  uint8_t const* end() const { return _start + byteSize(); }

  // No destructor, does not take part in memory management,

  // get the type for the slice
  inline ValueType type() const noexcept {
    return SliceStaticData::TypeMap[head()];
  }
  
  // get the type for the slice
  inline ValueType type(uint8_t h) const noexcept {
    return SliceStaticData::TypeMap[h];
  }

  char const* typeName() const { return valueTypeName(type()); }

  // pointer to the head byte
  uint8_t const* start() const noexcept { return _start; }

  // Set new memory position
  void set(uint8_t const* s) { _start = s; }

  // pointer to the head byte
  template <typename T>
  T const* startAs() const {
    return reinterpret_cast<T const*>(_start);
  }

  // value of the head byte
  inline uint8_t head() const noexcept { return *_start; }

  // hashes the binary representation of a value
  inline uint64_t hash(uint64_t seed = 0xdeadbeef) const {
    return VELOCYPACK_HASH(start(), checkOverflow(byteSize()), seed);
  }

  // hashes the value, normalizing different representations of
  // arrays, objects and numbers. this function may produce different
  // hash values than the binary hash() function
  uint64_t normalizedHash(uint64_t seed = 0xdeadbeef) const;

  // hashes the binary representation of a String slice. No check
  // is done if the Slice value is actually of type String
  inline uint64_t hashString(uint64_t seed = 0xdeadbeef) const noexcept {
    return VELOCYPACK_HASH(start(), static_cast<size_t>(stringSliceLength()), seed);
  }

  // check if slice is of the specified type
  inline bool isType(ValueType t) const noexcept {
    return SliceStaticData::TypeMap[*_start] == t;
  }

  // check if slice is a None object
  bool isNone() const noexcept { return isType(ValueType::None); }
  
  // check if slice is an Illegal object
  bool isIllegal() const noexcept { return isType(ValueType::Illegal); }

  // check if slice is a Null object
  bool isNull() const noexcept { return isType(ValueType::Null); }

  // check if slice is a Bool object
  bool isBool() const noexcept { return isType(ValueType::Bool); }

  // check if slice is a Bool object - this is an alias for isBool()
  bool isBoolean() const noexcept { return isBool(); }

  // check if slice is the Boolean value true
  bool isTrue() const noexcept { return head() == 0x1a; }

  // check if slice is the Boolean value false
  bool isFalse() const noexcept { return head() == 0x19; }

  // check if slice is an Array object
  bool isArray() const noexcept { return isType(ValueType::Array); }

  // check if slice is an Object object
  bool isObject() const noexcept { return isType(ValueType::Object); }

  // check if slice is a Double object
  bool isDouble() const noexcept { return isType(ValueType::Double); }

  // check if slice is a UTCDate object
  bool isUTCDate() const noexcept { return isType(ValueType::UTCDate); }

  // check if slice is an External object
  bool isExternal() const noexcept { return isType(ValueType::External); }

  // check if slice is a MinKey object
  bool isMinKey() const noexcept { return isType(ValueType::MinKey); }

  // check if slice is a MaxKey object
  bool isMaxKey() const noexcept { return isType(ValueType::MaxKey); }

  // check if slice is an Int object
  bool isInt() const noexcept { return isType(ValueType::Int); }

  // check if slice is a UInt object
  bool isUInt() const noexcept { return isType(ValueType::UInt); }

  // check if slice is a SmallInt object
  bool isSmallInt() const noexcept { return isType(ValueType::SmallInt); }

  // check if slice is a String object
  bool isString() const noexcept { return isType(ValueType::String); }

  // check if slice is a Binary object
  bool isBinary() const noexcept { return isType(ValueType::Binary); }

  // check if slice is a BCD
  bool isBCD() const noexcept { return isType(ValueType::BCD); }

  // check if slice is a Custom type
  bool isCustom() const noexcept { return isType(ValueType::Custom); }

  // check if a slice is any number type
  bool isInteger() const noexcept {
    return (isInt() || isUInt() || isSmallInt());
  }

  // check if slice is any Number-type object
  bool isNumber() const noexcept { return isInteger() || isDouble(); }
 
  // check if slice is convertible to a variable of a certain
  // number type 
  template<typename T>
  bool isNumber() const noexcept {
    try { 
      if (std::is_integral<T>()) {
        if (std::is_signed<T>()) {
          // signed integral type
          if (isDouble()) {
            auto v = getDouble();
            return (v >= static_cast<double>((std::numeric_limits<T>::min)()) &&
                    v <= static_cast<double>((std::numeric_limits<T>::max)()));
          }

          int64_t v = getInt();
          return (v >= static_cast<int64_t>((std::numeric_limits<T>::min)()) &&
                  v <= static_cast<int64_t>((std::numeric_limits<T>::max)()));
        } else {
          // unsigned integral type
          if (isDouble()) {
            auto v = getDouble();
            return (v >= 0.0 && v <= static_cast<double>(UINT64_MAX) &&
                    v <= static_cast<double>((std::numeric_limits<T>::max)()));
          }

          // may throw if value is < 0
          uint64_t v = getUInt();
          return (v <= static_cast<uint64_t>((std::numeric_limits<T>::max)()));
        }
      }
      
      // floating point type
      return isNumber();
    } catch (...) {
      // something went wrong
      return false;
    }
  }


  bool isSorted() const noexcept {
    auto const h = head();
    return (h >= 0x0b && h <= 0x0e);
  }

  // return the value for a Bool object
  bool getBool() const {
    if (!isBool()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Bool");
    }
    return isTrue();
  }

  // return the value for a Bool object - this is an alias for getBool()
  bool getBoolean() const { return getBool(); }

  // return the value for a Double object
  double getDouble() const {
    if (!isDouble()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Double");
    }
    union {
      uint64_t dv;
      double d;
    } v;
    v.dv = readIntegerFixed<uint64_t, 8>(_start + 1);
    return v.d;
  }

  // extract the array value at the specified index
  // - 0x02      : array without index table (all subitems have the same
  //               byte length), bytelen 1 byte, no number of subvalues
  // - 0x03      : array without index table (all subitems have the same
  //               byte length), bytelen 2 bytes, no number of subvalues
  // - 0x04      : array without index table (all subitems have the same
  //               byte length), bytelen 4 bytes, no number of subvalues
  // - 0x05      : array without index table (all subitems have the same
  //               byte length), bytelen 8 bytes, no number of subvalues
  // - 0x06      : array with 1-byte index table entries
  // - 0x07      : array with 2-byte index table entries
  // - 0x08      : array with 4-byte index table entries
  // - 0x09      : array with 8-byte index table entries
  Slice at(ValueLength index) const {
    if (!isArray()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Array");
    }

    return getNth(index);
  }

  Slice operator[](ValueLength index) const { return at(index); }

  // return the number of members for an Array or Object object
  ValueLength length() const {
    if (!isArray() && !isObject()) {
      throw Exception(Exception::InvalidValueType,
                      "Expecting type Array or Object");
    }

    auto const h = head();
    if (h == 0x01 || h == 0x0a) {
      // special case: empty!
      return 0;
    }

    if (h == 0x13 || h == 0x14) {
      // compact Array or Object
      ValueLength end = readVariableValueLength<false>(_start + 1);
      return readVariableValueLength<true>(_start + end - 1);
    }

    ValueLength const offsetSize = indexEntrySize(h);
    VELOCYPACK_ASSERT(offsetSize > 0);
    ValueLength end = readIntegerNonEmpty<ValueLength>(_start + 1, offsetSize);

    // find number of items
    if (h <= 0x05) {  // No offset table or length, need to compute:
      ValueLength firstSubOffset = findDataOffset(h);
      Slice first(_start + firstSubOffset);
      ValueLength s = first.byteSize();
      if (s == 0) {
        throw Exception(Exception::InternalError);
      }
      return (end - firstSubOffset) / s;
    } else if (offsetSize < 8) {
      return readIntegerNonEmpty<ValueLength>(_start + offsetSize + 1, offsetSize);
    }

    return readIntegerNonEmpty<ValueLength>(_start + end - offsetSize, offsetSize);
  }

  // extract a key from an Object at the specified index
  // - 0x0a      : empty object
  // - 0x0b      : object with 1-byte index table entries, sorted by attribute
  // name
  // - 0x0c      : object with 2-byte index table entries, sorted by attribute
  // name
  // - 0x0d      : object with 4-byte index table entries, sorted by attribute
  // name
  // - 0x0e      : object with 8-byte index table entries, sorted by attribute
  // name
  // - 0x0f      : object with 1-byte index table entries, not sorted by
  // attribute name
  // - 0x10      : object with 2-byte index table entries, not sorted by
  // attribute name
  // - 0x11      : object with 4-byte index table entries, not sorted by
  // attribute name
  // - 0x12      : object with 8-byte index table entries, not sorted by
  // attribute name
  Slice keyAt(ValueLength index, bool translate = true) const {
    if (!isObject()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Object");
    }

    return getNthKey(index, translate);
  }

  Slice valueAt(ValueLength index) const {
    if (!isObject()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Object");
    }

    Slice key = getNthKey(index, false);
    return Slice(key.start() + key.byteSize());
  }
  
  // extract the nth value from an Object
  Slice getNthValue(ValueLength index) const {
    Slice key = getNthKey(index, false);
    return Slice(key.start() + key.byteSize());
  }

  // look for the specified attribute path inside an Object
  // returns a Slice(ValueType::None) if not found
  Slice get(std::vector<std::string> const& attributes, 
            bool resolveExternals = false) const {
    size_t const n = attributes.size();
    if (n == 0) {
      throw Exception(Exception::InvalidAttributePath);
    }

    // use ourselves as the starting point
    Slice last = Slice(start());
    if (resolveExternals) {
      last = last.resolveExternal();
    }
    for (size_t i = 0; i < attributes.size(); ++i) {
      // fetch subattribute
      last = last.get(attributes[i]);

      // abort as early as possible
      if (last.isExternal()) {
        last = last.resolveExternal();
      }

      if (last.isNone() || (i + 1 < n && !last.isObject())) {
        return Slice();
      }
    }

    return last;
  }
  
  // look for the specified attribute path inside an Object
  // returns a Slice(ValueType::None) if not found
  Slice get(std::vector<char const*> const& attributes) const {
    size_t const n = attributes.size();
    if (n == 0) {
      throw Exception(Exception::InvalidAttributePath);
    }

    // use ourselves as the starting point
    Slice last = Slice(start());
    for (size_t i = 0; i < attributes.size(); ++i) {
      // fetch subattribute
      last = last.get(attributes[i]);

      // abort as early as possible
      if (last.isNone() || (i + 1 < n && !last.isObject())) {
        return Slice();
      }
    }

    return last;
  }

  // look for the specified attribute inside an Object
  // returns a Slice(ValueType::None) if not found
  Slice get(std::string const& attribute) const;
  
  // look for the specified attribute inside an Object
  // returns a Slice(ValueType::None) if not found
  Slice get(char const* attribute) const {
    return get(std::string(attribute));
  }

  Slice operator[](std::string const& attribute) const {
    return get(attribute);
  }

  // whether or not an Object has a specific key
  bool hasKey(std::string const& attribute) const {
    return !get(attribute).isNone();
  }

  // whether or not an Object has a specific sub-key
  bool hasKey(std::vector<std::string> const& attributes) const {
    return !get(attributes).isNone();
  }

  // return the pointer to the data for an External object
  char const* getExternal() const {
    if (!isExternal()) {
      throw Exception(Exception::InvalidValueType, "Expecting type External");
    }
    return extractPointer();
  }
  
  // returns the Slice managed by an External or the Slice itself if it's not
  // an External
  Slice resolveExternal() const {
    if (*_start == 0x1d) {
      return Slice(extractPointer());
    }
    return *this;
  }
 
  // returns the Slice managed by an External or the Slice itself if it's not
  // an External, recursive version
  Slice resolveExternals() const {
    char const* current = reinterpret_cast<char const*>(_start);
    while (*current == 0x1d) {
      current = Slice(current).extractPointer();
    }
    return Slice(current);
  }

  // tests whether the Slice is an empty array
  bool isEmptyArray() const noexcept { return head() == 0x01; }

  // tests whether the Slice is an empty object
  bool isEmptyObject() const noexcept { return head() == 0x0a; }

  // translates an integer key into a string
  Slice translate() const;
 
  // return the value for an Int object
  int64_t getInt() const;

  // return the value for a UInt object
  uint64_t getUInt() const;

  // return the value for a SmallInt object
  int64_t getSmallInt() const;
  
  template <typename T>
  T getNumber() const {
    if (std::is_integral<T>()) {
      if (std::is_signed<T>()) {
        // signed integral type
        if (isDouble()) {
          auto v = getDouble();
          if (v < static_cast<double>((std::numeric_limits<T>::min)()) ||
              v > static_cast<double>((std::numeric_limits<T>::max)())) {
            throw Exception(Exception::NumberOutOfRange);
          }
          return static_cast<T>(v);
        }

        int64_t v = getInt();
        if (v < static_cast<int64_t>((std::numeric_limits<T>::min)()) ||
            v > static_cast<int64_t>((std::numeric_limits<T>::max)())) {
          throw Exception(Exception::NumberOutOfRange);
        }
        return static_cast<T>(v);
      } else {
        // unsigned integral type
        if (isDouble()) {
          auto v = getDouble();
          if (v < 0.0 || v > static_cast<double>(UINT64_MAX) ||
              v > static_cast<double>((std::numeric_limits<T>::max)())) {
            throw Exception(Exception::NumberOutOfRange);
          }
          return static_cast<T>(v);
        }

        // may fail if number is < 0!
        uint64_t v = getUInt();
        if (v > static_cast<uint64_t>((std::numeric_limits<T>::max)())) {
          throw Exception(Exception::NumberOutOfRange);
        }
        return static_cast<T>(v);
      }
    }

    // floating point type

    if (isDouble()) {
      return static_cast<T>(getDouble());
    }
    if (isInt() || isSmallInt()) {
      return static_cast<T>(getIntUnchecked());
    }
    if (isUInt()) {
      return static_cast<T>(getUIntUnchecked());
    }

    throw Exception(Exception::InvalidValueType, "Expecting numeric type");
  }

  // an alias for getNumber<T>
  template <typename T>
  T getNumericValue() const {
    return getNumber<T>();
  }

  // return the value for a UTCDate object
  int64_t getUTCDate() const {
    if (!isUTCDate()) {
      throw Exception(Exception::InvalidValueType, "Expecting type UTCDate");
    }
    uint64_t v = readIntegerFixed<uint64_t, sizeof(uint64_t)>(_start + 1);
    return toInt64(v);
  }

  // return the value for a String object
  char const* getString(ValueLength& length) const {
    uint8_t const h = head();
    if (h >= 0x40 && h <= 0xbe) {
      // short UTF-8 String
      length = h - 0x40;
      return reinterpret_cast<char const*>(_start + 1);
    }

    if (h == 0xbf) {
      // long UTF-8 String
      length = readIntegerFixed<ValueLength, 8>(_start + 1);
      checkOverflow(length);
      return reinterpret_cast<char const*>(_start + 1 + 8);
    }

    throw Exception(Exception::InvalidValueType, "Expecting type String");
  }

  // return the length of the String slice
  ValueLength getStringLength() const {
    uint8_t const h = head();

    if (h >= 0x40 && h <= 0xbe) {
      // short UTF-8 String
      return h - 0x40;
    }

    if (h == 0xbf) {
      // long UTF-8 String
      return readIntegerFixed<ValueLength, 8>(_start + 1);
    }

    throw Exception(Exception::InvalidValueType, "Expecting type String");
  }

  // return a copy of the value for a String object
  std::string copyString() const {
    uint8_t h = head();
    if (h >= 0x40 && h <= 0xbe) {
      // short UTF-8 String
      ValueLength length = h - 0x40;
      return std::string(reinterpret_cast<char const*>(_start + 1),
                         static_cast<size_t>(length));
    }

    if (h == 0xbf) {
      ValueLength length = readIntegerFixed<ValueLength, 8>(_start + 1);
      return std::string(reinterpret_cast<char const*>(_start + 1 + 8),
                         checkOverflow(length));
    }

    throw Exception(Exception::InvalidValueType, "Expecting type String");
  }

  // return the value for a Binary object
  uint8_t const* getBinary(ValueLength& length) const {
    if (!isBinary()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Binary");
    }

    uint8_t const h = head();
    VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);

    length = readIntegerNonEmpty<ValueLength>(_start + 1, h - 0xbf);
    checkOverflow(length);
    return _start + 1 + h - 0xbf;
  }

  // return the length of the Binary slice
  ValueLength getBinaryLength() const {
    if (!isBinary()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Binary");
    }

    uint8_t const h = head();
    VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);

    return readIntegerNonEmpty<ValueLength>(_start + 1, h - 0xbf);
  }

  // return a copy of the value for a Binary object
  std::vector<uint8_t> copyBinary() const {
    if (!isBinary()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Binary");
    }

    uint8_t const h = head();
    VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);

    std::vector<uint8_t> out;
    ValueLength length = readIntegerNonEmpty<ValueLength>(_start + 1, h - 0xbf);
    checkOverflow(length);
    out.reserve(static_cast<size_t>(length));
    out.insert(out.end(), _start + 1 + h - 0xbf,
               _start + 1 + h - 0xbf + length);
    return out;
  }

  // get the total byte size for the slice, including the head byte
  ValueLength byteSize() const {
    auto const h = head();
    // check if the type has a fixed length first
    ValueLength l = SliceStaticData::FixedTypeLengths[h];
    if (l != 0) {
      // return fixed length
      return l;
    }

    // types with dynamic lengths need special treatment:
    switch (type(h)) {
      case ValueType::Array:
      case ValueType::Object: {
        if (h == 0x13 || h == 0x14) {
          // compact Array or Object
          return readVariableValueLength<false>(_start + 1);
        }

        if (h == 0x01 || h == 0x0a) {
          // we cannot get here, because the FixedTypeLengths lookup
          // above will have kicked in already. however, the compiler
          // claims we'll be reading across the bounds of the input
          // here...
          return 1;
        }

        VELOCYPACK_ASSERT(h > 0x00 && h <= 0x0e);
        if (h >= sizeof(SliceStaticData::WidthMap) / sizeof(SliceStaticData::WidthMap[0])) {
          throw Exception(Exception::InternalError, "invalid Array/Object type");
        }
        return readIntegerNonEmpty<ValueLength>(_start + 1,
                                                SliceStaticData::WidthMap[h]);
      }

      case ValueType::External: {
        return 1 + sizeof(char*);
      }

      case ValueType::UTCDate: {
        return 1 + sizeof(int64_t);
      }

      case ValueType::Int: {
        return static_cast<ValueLength>(1 + (h - 0x1f));
      }

      case ValueType::String: {
        VELOCYPACK_ASSERT(h == 0xbf);
        if (h < 0xbf) {
          // we cannot get here, because the FixedTypeLengths lookup
          // above will have kicked in already. however, the compiler
          // claims we'll be reading across the bounds of the input
          // here...
          return h - 0x40;
        }
        // long UTF-8 String
        return static_cast<ValueLength>(
            1 + 8 + readIntegerFixed<ValueLength, 8>(_start + 1));
      }

      case ValueType::Binary: {
        VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);
        return static_cast<ValueLength>(
            1 + h - 0xbf + readIntegerNonEmpty<ValueLength>(_start + 1, h - 0xbf));
      }

      case ValueType::BCD: {
        if (h <= 0xcf) {
          // positive BCD
          VELOCYPACK_ASSERT(h >= 0xc8 && h < 0xcf);
          return static_cast<ValueLength>(
              1 + h - 0xc7 + readIntegerNonEmpty<ValueLength>(_start + 1, h - 0xc7));
        }

        // negative BCD
        VELOCYPACK_ASSERT(h >= 0xd0 && h < 0xd7);
        return static_cast<ValueLength>(
            1 + h - 0xcf + readIntegerNonEmpty<ValueLength>(_start + 1, h - 0xcf));
      }

      case ValueType::Custom: {
        VELOCYPACK_ASSERT(h >= 0xf4);
        switch (h) {
          case 0xf4: 
          case 0xf5: 
          case 0xf6: {
            return 2 + readIntegerFixed<ValueLength, 1>(_start + 1);
          }

          case 0xf7: 
          case 0xf8: 
          case 0xf9:  {
            return 3 + readIntegerFixed<ValueLength, 2>(_start + 1);
          }
          
          case 0xfa: 
          case 0xfb: 
          case 0xfc: {
            return 5 + readIntegerFixed<ValueLength, 4>(_start + 1);
          }
          
          case 0xfd: 
          case 0xfe: 
          case 0xff: {
            return 9 + readIntegerFixed<ValueLength, 8>(_start + 1);
          }

          default: {
            // fallthrough intentional
          }
        }
      }
      default: {
        // fallthrough intentional
      }
    }

    throw Exception(Exception::InternalError);
  }
  
  ValueLength findDataOffset(uint8_t head) const noexcept {
    // Must be called for a nonempty array or object at start():
    VELOCYPACK_ASSERT(head <= 0x12);
    unsigned int fsm = SliceStaticData::FirstSubMap[head];
    if (fsm <= 2 && _start[2] != 0) {
      return 2;
    }
    if (fsm <= 3 && _start[3] != 0) {
      return 3;
    }
    if (fsm <= 5 && _start[5] != 0) {
      return 5;
    }
    return 9;
  }
  
  // get the offset for the nth member from an Array type
  ValueLength getNthOffset(ValueLength index) const;

  Slice makeKey() const;

  int compareString(char const* value, size_t length) const;
  
  inline int compareString(std::string const& attribute) const {
    return compareString(attribute.c_str(), attribute.size());
  }

  bool isEqualString(std::string const& attribute) const;

  // check if two Slices are equal on the binary level
  bool equals(Slice const& other) const {
    if (head() != other.head()) {
      return false;
    }

    ValueLength const size = byteSize();

    if (size != other.byteSize()) {
      return false;
    }

    return (memcmp(start(), other.start(),
                  arangodb::velocypack::checkOverflow(size)) == 0);
  }
  
  bool operator==(Slice const& other) const { return equals(other); }
  bool operator!=(Slice const& other) const { return !equals(other); }

  static bool equals(uint8_t const* left, uint8_t const* right) {
    return Slice(left).equals(Slice(right));
  }

  std::string toHex() const;
  std::string toJson(Options const* options = &Options::Defaults) const;
  std::string toString(Options const* options = &Options::Defaults) const;
  std::string hexType() const;
  
  int64_t getIntUnchecked() const;

  // return the value for a UInt object, without checks
  // returns 0 for invalid values/types
  uint64_t getUIntUnchecked() const;
  
 private:
  // get the total byte size for a String slice, including the head byte
  // not check is done if the type of the slice is actually String 
  ValueLength stringSliceLength() const noexcept {
    // check if the type has a fixed length first
    auto const h = head();
    if (h == 0xbf) {
      // long UTF-8 String
      return static_cast<ValueLength>(
        1 + 8 + readIntegerFixed<ValueLength, 8>(_start + 1));
    }
    return static_cast<ValueLength>(1 + h - 0x40);
  }
  
  // translates an integer key into a string, without checks
  Slice translateUnchecked() const;

  Slice getFromCompactObject(std::string const& attribute) const;

  // extract the nth member from an Array
  Slice getNth(ValueLength index) const;

  // extract the nth member from an Object, note that this is the nth
  // entry in the hash table for types 0x0b to 0x0e
  Slice getNthKey(ValueLength index, bool translate) const;

  // get the offset for the nth member from a compact Array or Object type
  ValueLength getNthOffsetFromCompact(ValueLength index) const;

  inline ValueLength indexEntrySize(uint8_t head) const noexcept {
    VELOCYPACK_ASSERT(head > 0x00 && head <= 0x12);
    return static_cast<ValueLength>(SliceStaticData::WidthMap[head]);
  }

  // perform a linear search for the specified attribute inside an Object
  Slice searchObjectKeyLinear(std::string const& attribute, ValueLength ieBase,
                              ValueLength offsetSize, ValueLength n) const;

  // perform a binary search for the specified attribute inside an Object
  template<ValueLength offsetSize>
  Slice searchObjectKeyBinary(std::string const& attribute, ValueLength ieBase, ValueLength n) const;

// assert that the slice is of a specific type
// can be used for debugging and removed in production
#ifdef VELOCYPACK_ASSERT
  inline void assertType(ValueType) const {}
#else
  inline void assertType(ValueType type) const {
    VELOCYPACK_ASSERT(this->type() == type);
  }
#endif

  // extracts a pointer from the slice and converts it into a
  // built-in pointer type
  char const* extractPointer() const {
    union {
      char const* value;
      char binary[sizeof(char const*)];
    };
    memcpy(&binary[0], _start + 1, sizeof(char const*));
    return value;
  }
};

// a class for keeping Slice allocations in scope
class SliceScope {
 public:
  SliceScope(SliceScope const&) = delete;
  SliceScope& operator=(SliceScope const&) = delete;
  SliceScope();
  ~SliceScope();

  Slice add(uint8_t const* data, ValueLength size);

 private:
  std::vector<uint8_t*> _allocations;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

namespace std {
// implementation of std::hash for a Slice object
template <>
struct hash<arangodb::velocypack::Slice> {
  size_t operator()(arangodb::velocypack::Slice const& slice) const {
#ifdef VELOCYPACK_32BIT
    // size_t is only 32 bits wide here... so don't simply truncate the
    // 64 bit hash value but convert it into a 32 bit value using data
    // from low and high bytes
    uint64_t const hash = slice.hash();
    return static_cast<uint32_t>(hash >> 32) ^ static_cast<uint32_t>(hash);
#else
    return static_cast<size_t>(slice.hash());
#endif
  }
};

}

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Slice const*);

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Slice const&);

#endif

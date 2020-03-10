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

#if __cplusplus >= 201703L
#include <string_view>
#define VELOCYPACK_HAS_STRING_VIEW 1
#endif

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"
#include "velocypack/Options.h"
#include "velocypack/SliceStaticData.h"
#include "velocypack/StringRef.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"

namespace arangodb {
namespace velocypack {

// This class provides read only access to a VPack value, it is
// intentionally light-weight (only one pointer value), such that
// it can easily be used to traverse larger VPack values.

// A Slice does not own the VPack data it points to!
class Slice {
  friend class Builder;
  friend class ArrayIterator;
  friend class ObjectIterator;
  friend class ValueSlice;

  // _start is the pointer to the first byte of the value. It should always be
  // accessed through the start() method as that allows subclasses to adjust
  // the start position should it be necessary. For example, the ValueSlice
  // class uses this to make tags transparent and behaves as if they did not
  // exist, unless explicitly queried for.
  uint8_t const* _start;

 public:
  static constexpr uint64_t defaultSeed64 = 0xdeadbeef;
  static constexpr uint32_t defaultSeed32 = 0xdeadbeef;
  static constexpr uint64_t defaultSeed = defaultSeed64;
 
  static uint8_t const noneSliceData[];
  static uint8_t const illegalSliceData[];
  static uint8_t const nullSliceData[];
  static uint8_t const falseSliceData[];
  static uint8_t const trueSliceData[];
  static uint8_t const zeroSliceData[];
  static uint8_t const emptyStringSliceData[];
  static uint8_t const emptyArraySliceData[];
  static uint8_t const emptyObjectSliceData[];
  static uint8_t const minKeySliceData[];
  static uint8_t const maxKeySliceData[];

  // constructor for an empty Value of type None
  constexpr Slice() noexcept : Slice(noneSliceData) {}

  // creates a Slice from a pointer to a uint8_t array
  explicit constexpr Slice(uint8_t const* start) noexcept
      : _start(start) {}
  
  // No destructor, does not take part in memory management

  // creates a slice of type None
  static constexpr Slice noneSlice() noexcept { return Slice(noneSliceData); }
  
  // creates a slice of type Illegal
  static constexpr Slice illegalSlice() noexcept { return Slice(illegalSliceData); }

  // creates a slice of type Null
  static constexpr Slice nullSlice() noexcept { return Slice(nullSliceData); }
  
  // creates a slice of type Boolean with the relevant value
  static constexpr Slice booleanSlice(bool value) noexcept { return value ? trueSlice() : falseSlice(); }

  // creates a slice of type Boolean with false value
  static constexpr Slice falseSlice() noexcept { return Slice(falseSliceData); }

  // creates a slice of type Boolean with true value
  static constexpr Slice trueSlice() noexcept { return Slice(trueSliceData); }
  
  // creates a slice of type Smallint(0)
  static constexpr Slice zeroSlice() noexcept { return Slice(zeroSliceData); }
  
  // creates a slice of type String, empty
  static constexpr Slice emptyStringSlice() noexcept { return Slice(emptyStringSliceData); }
  
  // creates a slice of type Array, empty
  static constexpr Slice emptyArraySlice() noexcept { return Slice(emptyArraySliceData); }
  
  // creates a slice of type Object, empty
  static constexpr Slice emptyObjectSlice() noexcept { return Slice(emptyObjectSliceData); }
  
  // creates a slice of type MinKey
  static constexpr Slice minKeySlice() noexcept { return Slice(minKeySliceData); }

  // creates a slice of type MaxKey
  static constexpr Slice maxKeySlice() noexcept { return Slice(maxKeySliceData); }

  constexpr Slice const value() const noexcept {
    return isTagged() ? Slice(valueStart()) : *this;
  }

  constexpr uint64_t getFirstTag() const {
    // always need the actual first byte, so use _start directly
    return !isTagged() ? 0 :
             (*_start == 0xee ? readIntegerFixed<uint64_t, 1>(_start + 1) :
               (*_start == 0xef ? readIntegerFixed<uint64_t, 8>(_start + 1) : /* error */ 0)
             );
  }

  std::vector<uint64_t> getTags() const {
    std::vector<uint64_t> ret;

    if (isTagged()) {
      // always need the actual first byte, so use _start directly
      uint8_t const* start = _start;

      while (SliceStaticData::TypeMap[*start] == ValueType::Tagged) {
        uint8_t offset;
        uint64_t tag;

        if (*start == 0xee) {
          tag = readIntegerFixed<uint64_t, 1>(start + 1);
          offset = 2;
        } else if (*start == 0xef) {
          tag = readIntegerFixed<uint64_t, 8>(start + 1);
          offset = 9;
        } else {
          throw new Exception(Exception::InternalError, "Invalid tag type ID");
        }

        ret.push_back(tag);
        start += offset;
      }
    }

    return ret;
  }

  bool hasTag(uint64_t tagId) const {
    // always need the actual first byte, so use _start directly
    uint8_t const* start = _start;

    while (SliceStaticData::TypeMap[*start] == ValueType::Tagged) {
      uint8_t offset;
      uint64_t tag;

      if (*start == 0xee) {
        tag = readIntegerFixed<uint64_t, 1>(start + 1);
        offset = 2;
      } else if (*start == 0xef) {
        tag = readIntegerFixed<uint64_t, 8>(start + 1);
        offset = 9;
      } else {
        throw new Exception(Exception::InternalError, "Invalid tag type ID");
      }

      if (tag == tagId) {
        return true;
      }

      start += offset;
    }

    return false;
  }

  // pointer to the head byte, excluding possible tags
  uint8_t const* valueStart() const noexcept {
    // always need the actual first byte, so use _start directly
    return _start + tagsOffset(_start);
  }

  // pointer to the head byte, including possible tags
  // other implementations may exclude tags
  constexpr uint8_t const* start() const noexcept {
    return _start;
  }

  // pointer to the head byte
  template <typename T>
  T const* startAs() const {
    return reinterpret_cast<T const*>(start());
  }

  // value of the head byte
  constexpr inline uint8_t head() const noexcept { return *start(); }

  uint8_t const* begin() noexcept { return start(); }

  constexpr uint8_t const* begin() const noexcept { return start(); }

  uint8_t const* end() const { return start() + byteSize(); }

  // get the type for the slice
  constexpr inline ValueType type() const noexcept {
    return type(head());
  }

  // get the type name for the slice
  char const* typeName() const { return valueTypeName(type()); }

  // Set new memory position
  void set(uint8_t const* s) { _start = s; }

  // hashes the binary representation of a value
  inline uint64_t hash(uint64_t seed = defaultSeed64) const {
    std::size_t const size = checkOverflow(byteSize());
    if (seed == defaultSeed64 && size == 1) {
      uint64_t h = SliceStaticData::PrecalculatedHashesForDefaultSeed[head()];
      VELOCYPACK_ASSERT(h != 0);
      return h;
    }
    return VELOCYPACK_HASH(start(), size, seed);
  }

  // hashes the binary representation of a value
  inline uint32_t hash32(uint32_t seed = defaultSeed32) const {
    size_t const size = checkOverflow(byteSize());
    return VELOCYPACK_HASH32(start(), size, seed);
  }

  // hashes the binary representation of a value, not using precalculated hash values
  // this is mainly here for testing purposes
  inline uint64_t hashSlow(uint64_t seed = defaultSeed64) const {
    std::size_t const size = checkOverflow(byteSize());
    return VELOCYPACK_HASH(start(), size, seed);
  }

  // hashes the value, normalizing different representations of
  // arrays, objects and numbers. this function may produce different
  // hash values than the binary hash() function
  uint64_t normalizedHash(uint64_t seed = defaultSeed64) const;

  // hashes the value, normalizing different representations of
  // arrays, objects and numbers. this function may produce different
  // hash values than the binary hash32() function
  uint32_t normalizedHash32(uint32_t seed = defaultSeed32) const;

  // hashes the binary representation of a String slice. No check
  // is done if the Slice value is actually of type String
  inline uint64_t hashString(uint64_t seed = defaultSeed64) const noexcept {
    return VELOCYPACK_HASH(start(), static_cast<std::size_t>(stringSliceLength()), seed);
  }

  // hashes the binary representation of a String slice. No check
  // is done if the Slice value is actually of type String
  inline uint32_t hashString32(uint32_t seed = defaultSeed32) const noexcept {
    return VELOCYPACK_HASH32(start(), static_cast<std::size_t>(stringSliceLength()), seed);
  }

  // check if slice is of the specified type (including tags)
  // other implementations may be excluding tags
  constexpr inline bool isType(ValueType t) const {
    return SliceStaticData::TypeMap[*start()] == t;
  }

  // check if slice is a None object
  constexpr bool isNone() const noexcept { return isType(ValueType::None); }
  
  // check if slice is an Illegal object
  constexpr bool isIllegal() const noexcept { return isType(ValueType::Illegal); }

  // check if slice is a Null object
  constexpr bool isNull() const noexcept { return isType(ValueType::Null); }

  // check if slice is a Bool object
  constexpr bool isBool() const noexcept { return isType(ValueType::Bool); }

  // check if slice is a Bool object - this is an alias for isBool()
  constexpr bool isBoolean() const noexcept { return isBool(); }

  // check if slice is the Boolean value true
  constexpr bool isTrue() const noexcept { return head() == 0x1a; }

  // check if slice is the Boolean value false
  constexpr bool isFalse() const noexcept { return head() == 0x19; }

  // check if slice is an Array object
  constexpr bool isArray() const noexcept { return isType(ValueType::Array); }

  // check if slice is an Object object
  constexpr bool isObject() const noexcept { return isType(ValueType::Object); }

  // check if slice is a Double object
  constexpr bool isDouble() const noexcept { return isType(ValueType::Double); }

  // check if slice is a UTCDate object
  constexpr bool isUTCDate() const noexcept { return isType(ValueType::UTCDate); }

  // check if slice is an External object
  constexpr bool isExternal() const noexcept { return isType(ValueType::External); }

  // check if slice is a MinKey object
  constexpr bool isMinKey() const noexcept { return isType(ValueType::MinKey); }

  // check if slice is a MaxKey object
  constexpr bool isMaxKey() const noexcept { return isType(ValueType::MaxKey); }

  // check if slice is an Int object
  constexpr bool isInt() const noexcept { return isType(ValueType::Int); }

  // check if slice is a UInt object
  constexpr bool isUInt() const noexcept { return isType(ValueType::UInt); }

  // check if slice is a SmallInt object
  constexpr bool isSmallInt() const noexcept { return isType(ValueType::SmallInt); }

  // check if slice is a String object
  constexpr bool isString() const noexcept { return isType(ValueType::String); }

  // check if slice is a Binary object
  constexpr bool isBinary() const noexcept { return isType(ValueType::Binary); }

  // check if slice is a BCD
  constexpr bool isBCD() const noexcept { return isType(ValueType::BCD); }

  // check if slice is a Custom type
  constexpr bool isCustom() const noexcept { return isType(ValueType::Custom); }

  // check if slice is a Tagged type
  constexpr bool isTagged() const noexcept { return isType(ValueType::Tagged); }

  // check if a slice is any number type
  constexpr bool isInteger() const noexcept {
    return (isInt() || isUInt() || isSmallInt());
  }

  // check if slice is any Number-type object
  constexpr bool isNumber() const noexcept { return isInteger() || isDouble(); }
 
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

  constexpr bool isSorted() const noexcept {
    return (head() >= 0x0b && head() <= 0x0e);
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
    v.dv = readIntegerFixed<uint64_t, 8>(start() + 1);
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
    if (VELOCYPACK_UNLIKELY(!isArray())) {
      throw Exception(Exception::InvalidValueType, "Expecting type Array");
    }

    return getNth(index);
  }

  Slice operator[](ValueLength index) const { return at(index); }

  // return the number of members for an Array or Object object
  ValueLength length() const {
    if (VELOCYPACK_UNLIKELY(!isArray() && !isObject())) {
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
      ValueLength end = readVariableValueLength<false>(start() + 1);
      return readVariableValueLength<true>(start() + end - 1);
    }

    ValueLength const offsetSize = indexEntrySize(h);
    VELOCYPACK_ASSERT(offsetSize > 0);
    ValueLength end = readIntegerNonEmpty<ValueLength>(start() + 1, offsetSize);

    // find number of items
    if (h <= 0x05) {  // No offset table or length, need to compute:
      VELOCYPACK_ASSERT(h != 0x00 && h != 0x01);
      ValueLength firstSubOffset = findDataOffset(h);
      Slice first(start() + firstSubOffset);
      ValueLength s = first.byteSize();
      if (s == 0) {
        throw Exception(Exception::InternalError, "Invalid data for Array");
      }
      return (end - firstSubOffset) / s;
    } else if (offsetSize < 8) {
      return readIntegerNonEmpty<ValueLength>(start() + offsetSize + 1, offsetSize);
    }

    return readIntegerNonEmpty<ValueLength>(start() + end - offsetSize, offsetSize);
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
    if (VELOCYPACK_UNLIKELY(!isObject())) {
      throw Exception(Exception::InvalidValueType, "Expecting type Object");
    }

    return getNthKey(index, translate);
  }

  Slice valueAt(ValueLength index) const {
    if (VELOCYPACK_UNLIKELY(!isObject())) {
      throw Exception(Exception::InvalidValueType, "Expecting type Object");
    }

    Slice key = getNthKeyUntranslated(index);
    return Slice(key.start() + key.byteSize());
  }
  
  // extract the nth value from an Object
  Slice getNthValue(ValueLength index) const {
    Slice key = getNthKeyUntranslated(index);
    return Slice(key.start() + key.byteSize());
  }

  // look for the specified attribute path inside an Object
  // returns a Slice(ValueType::None) if not found
  template<typename T>
  Slice get(std::vector<T> const& attributes, 
            bool resolveExternals = false) const {
    std::size_t const n = attributes.size();
    if (n == 0) {
      throw Exception(Exception::InvalidAttributePath);
    }

    // use ourselves as the starting point
    Slice last(start());
    if (resolveExternals) {
      last = last.resolveExternal();
    }
    for (std::size_t i = 0; i < attributes.size(); ++i) {
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
  
  // look for the specified attribute inside an Object
  // returns a Slice(ValueType::None) if not found
  Slice get(StringRef const& attribute) const;

  Slice get(std::string const& attribute) const {
    return get(StringRef(attribute.data(), attribute.size()));
  }

  Slice get(char const* attribute) const {
    return get(StringRef(attribute));
  }

  Slice get(char const* attribute, std::size_t length) const {
    return get(StringRef(attribute, length));
  }
  
  Slice operator[](StringRef const& attribute) const {
    return get(attribute);
  }

  Slice operator[](std::string const& attribute) const {
    return get(attribute.data(), attribute.size());
  }
  
  // whether or not an Object has a specific key
  bool hasKey(StringRef const& attribute) const {
    return !get(attribute).isNone();
  }

  bool hasKey(std::string const& attribute) const {
    return hasKey(StringRef(attribute));
  }
  
  bool hasKey(char const* attribute) const {
    return hasKey(StringRef(attribute));
  }
  
  bool hasKey(char const* attribute, std::size_t length) const {
    return hasKey(StringRef(attribute, length));
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
    if (*start() == 0x1d) {
      return Slice(reinterpret_cast<uint8_t const*>(extractPointer()));
    }
    return *this;
  }
 
  // returns the Slice managed by an External or the Slice itself if it's not
  // an External, recursive version
  Slice resolveExternals() const {
    uint8_t const* current = start();
    while (*current == 0x1d) {
      current = reinterpret_cast<uint8_t const*>(Slice(current).extractPointer());
    }
    return Slice(current);
  }

  // tests whether the Slice is an empty array
  bool isEmptyArray() const { 
    return isArray() && length() == 0;
  }

  // tests whether the Slice is an empty object
  bool isEmptyObject() const { 
    return isObject() && length() == 0;
  }

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
    uint64_t v = readIntegerFixed<uint64_t, sizeof(uint64_t)>(start() + 1);
    return toInt64(v);
  }

  // return the value for a String object
  char const* getString(ValueLength& length) const {
    uint8_t const h = head();
    if (h >= 0x40 && h <= 0xbe) {
      // short UTF-8 String
      length = h - 0x40;
      return reinterpret_cast<char const*>(start() + 1);
    }

    if (h == 0xbf) {
      // long UTF-8 String
      length = readIntegerFixed<ValueLength, 8>(start() + 1);
      checkOverflow(length);
      return reinterpret_cast<char const*>(start() + 1 + 8);
    }

    throw Exception(Exception::InvalidValueType, "Expecting type String");
  }
  
  char const* getStringUnchecked(ValueLength& length) const noexcept {
    uint8_t const h = head();
    if (h >= 0x40 && h <= 0xbe) {
      // short UTF-8 String
      length = h - 0x40;
      return reinterpret_cast<char const*>(start() + 1);
    }

    // long UTF-8 String
    length = readIntegerFixed<ValueLength, 8>(start() + 1);
    return reinterpret_cast<char const*>(start() + 1 + 8);
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
      return readIntegerFixed<ValueLength, 8>(start() + 1);
    }

    throw Exception(Exception::InvalidValueType, "Expecting type String");
  }

  // return a copy of the value for a String object
  std::string copyString() const {
    uint8_t h = head();
    if (h >= 0x40 && h <= 0xbe) {
      // short UTF-8 String
      ValueLength length = h - 0x40;
      return std::string(reinterpret_cast<char const*>(start() + 1),
                         static_cast<std::size_t>(length));
    }

    if (h == 0xbf) {
      ValueLength length = readIntegerFixed<ValueLength, 8>(start() + 1);
      return std::string(reinterpret_cast<char const*>(start() + 1 + 8),
                         checkOverflow(length));
    }

    throw Exception(Exception::InvalidValueType, "Expecting type String");
  }
  
  // return a copy of the value for a String object
  StringRef stringRef() const {
    uint8_t h = head();
    if (h >= 0x40 && h <= 0xbe) {
      // short UTF-8 String
      ValueLength length = h - 0x40;
      return StringRef(reinterpret_cast<char const*>(start() + 1),
                       static_cast<std::size_t>(length));
    }

    if (h == 0xbf) {
      ValueLength length = readIntegerFixed<ValueLength, 8>(start() + 1);
      return StringRef(reinterpret_cast<char const*>(start() + 1 + 8),
                       checkOverflow(length));
    }

    throw Exception(Exception::InvalidValueType, "Expecting type String");
  }
#ifdef VELOCYPACK_HAS_STRING_VIEW
  std::string_view stringView() const {
    StringRef ref  = this->stringRef();
    return std::string_view(ref.data(), ref.size());
  }
#endif

  // return the value for a Binary object
  uint8_t const* getBinary(ValueLength& length) const {
    if (!isBinary()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Binary");
    }

    uint8_t const h = head();
    VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);

    length = readIntegerNonEmpty<ValueLength>(start() + 1, h - 0xbf);
    checkOverflow(length);
    return start() + 1 + h - 0xbf;
  }

  // return the length of the Binary slice
  ValueLength getBinaryLength() const {
    if (!isBinary()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Binary");
    }

    uint8_t const h = head();
    VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);

    return readIntegerNonEmpty<ValueLength>(start() + 1, h - 0xbf);
  }

  // return a copy of the value for a Binary object
  std::vector<uint8_t> copyBinary() const {
    if (!isBinary()) {
      throw Exception(Exception::InvalidValueType, "Expecting type Binary");
    }

    uint8_t const h = head();
    VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);

    std::vector<uint8_t> out;
    ValueLength length = readIntegerNonEmpty<ValueLength>(start() + 1, h - 0xbf);
    checkOverflow(length);
    out.reserve(static_cast<std::size_t>(length));
    out.insert(out.end(), start() + 1 + h - 0xbf,
               start() + 1 + h - 0xbf + length);
    return out;
  }

  // get the total byte size for the slice, including the head byte, including tags
  ValueLength byteSize() const {
    return byteSize(start());
  }

  // get the total byte size for the slice, including the head byte, excluding tags
  ValueLength valueByteSize() const {
    return byteSize(valueStart());
  }

  ValueLength findDataOffset(uint8_t head) const noexcept {
    // Must be called for a non-empty array or object at start():
    VELOCYPACK_ASSERT(head != 0x01 && head != 0x0a && head <= 0x14);
    unsigned int fsm = SliceStaticData::FirstSubMap[head];
    uint8_t const* start = this->start();
    if (fsm == 0) {
      // need to calculate the offset by reading the dynamic length
      VELOCYPACK_ASSERT(head == 0x13 || head == 0x14);
      return 1 + arangodb::velocypack::getVariableValueLength(readVariableValueLength<false>(start + 1));
    }
    if (fsm <= 2 && start[2] != 0) {
      return 2;
    }
    if (fsm <= 3 && start[3] != 0) {
      return 3;
    }
    if (fsm <= 5 && start[5] != 0) {
      return 5;
    }
    return 9;
  }
  
  // get the offset for the nth member from an Array type
  ValueLength getNthOffset(ValueLength index) const;

  Slice makeKey() const;

  int compareString(StringRef const& value) const;
  
  inline int compareString(std::string const& value) const {
    return compareString(StringRef(value.data(), value.size()));
  }
  
  int compareString(char const* value, std::size_t length) const {
    return compareString(StringRef(value, length));
  }
  
  int compareStringUnchecked(StringRef const& value) const noexcept;
  
  int compareStringUnchecked(std::string const& value) const noexcept {
    return compareStringUnchecked(StringRef(value.data(), value.size()));
  }

  int compareStringUnchecked(char const* value, std::size_t length) const noexcept {
    return compareStringUnchecked(StringRef(value, length));
  }
  
  bool isEqualString(StringRef const& attribute) const;

  bool isEqualString(std::string const& attribute) const {
    return isEqualString(StringRef(attribute.data(), attribute.size()));
  }
  
  bool isEqualStringUnchecked(StringRef const& attribute) const noexcept;

  bool isEqualStringUnchecked(std::string const& attribute) const noexcept {
    return isEqualStringUnchecked(StringRef(attribute.data(), attribute.size()));
  }

  // check if two Slices are equal on the binary level
  // please note that for several values there are multiple possible representations,
  // which differ on the binary level but will still resolve to the same logical
  // values. For example, smallint(1) and int(1) are logically the same, but will
  // resolve to either 0x31 or 0x28 0x01.
  bool binaryEquals(Slice const& other) const {
    if (start() == other.start()) {
      // same underlying data, so the slices must be identical
      return true;
    }

    if (head() != other.head()) {
      return false;
    }

    ValueLength const size = byteSize();

    if (size != other.byteSize()) {
      return false;
    }

    return (memcmp(start(), other.start(), checkOverflow(size)) == 0);
  }
 
  static bool binaryEquals(uint8_t const* left, uint8_t const* right) {
    return Slice(left).binaryEquals(Slice(right));
  }
  
  // these operators are now deleted because they didn't do what people expected
  // these operators checked for _binary_ equality of the velocypack slice with
  // another. however, for several values there are multiple possible representations,
  // which differ on the binary level but will still resolve to the same logical
  // values. For example, smallint(1) and int(1) are logically the same, but will
  // resolve to either 0x31 or 0x28 0x01.
  bool operator==(Slice const& other) const = delete;
  bool operator!=(Slice const& other) const = delete;

  std::string toHex() const;
  std::string toJson(Options const* options = &Options::Defaults) const;
  std::string toString(Options const* options = &Options::Defaults) const;
  std::string hexType() const;
  
  int64_t getIntUnchecked() const noexcept;

  // return the value for a UInt object, without checks
  // returns 0 for invalid values/types
  uint64_t getUIntUnchecked() const noexcept;

  // return the value for a SmallInt object, without checks
  // returns 0 for invalid values/types
  int64_t getSmallIntUnchecked() const noexcept;

  uint8_t const* getBCD(int8_t& sign, int32_t& exponent, ValueLength& mantissaLength) const {
    if (VELOCYPACK_UNLIKELY(!isBCD())) {
      throw Exception(Exception::InvalidValueType, "Expecting type BCD");
    }

    uint64_t type = head();
    bool positive = type >= 0xc8 && type <= 0xcf;
    uint64_t mlenlen = type - (positive ? 0xc7 : 0xcf);

    sign = positive ? 1 : -1;
    exponent = static_cast<int32_t>(readIntegerFixed<uint32_t, 4>(valueStart() + 1 + mlenlen));
    mantissaLength = readIntegerNonEmpty<ValueLength>(valueStart() + 1, mlenlen);

    return valueStart() + 1 + mlenlen + 4;
  }

 private:
  // get the type for the slice (including tags)
  static constexpr inline ValueType type(uint8_t h) {
    return SliceStaticData::TypeMap[h];
  }

  // return the number of members for an Array
  // must only be called for Slices that have been validated to be of type Array
  ValueLength arrayLength() const {
    auto const h = head();
    VELOCYPACK_ASSERT(type(h) == ValueType::Array);

    if (h == 0x01) {
      // special case: empty!
      return 0;
    }

    if (h == 0x13) {
      // compact Array
      ValueLength end = readVariableValueLength<false>(start() + 1);
      return readVariableValueLength<true>(start() + end - 1);
    }

    ValueLength const offsetSize = indexEntrySize(h);
    VELOCYPACK_ASSERT(offsetSize > 0);

    // find number of items
    if (h <= 0x05) {  // No offset table or length, need to compute:
      VELOCYPACK_ASSERT(h != 0x00 && h != 0x01);
      ValueLength firstSubOffset = findDataOffset(h);
      Slice first(start() + firstSubOffset);
      ValueLength s = first.byteSize();
      if (VELOCYPACK_UNLIKELY(s == 0)) {
        throw Exception(Exception::InternalError, "Invalid data for Array");
      }
      ValueLength end = readIntegerNonEmpty<ValueLength>(start() + 1, offsetSize);
      return (end - firstSubOffset) / s;
    } else if (offsetSize < 8) {
      return readIntegerNonEmpty<ValueLength>(start() + offsetSize + 1, offsetSize);
    }

    ValueLength end = readIntegerNonEmpty<ValueLength>(start() + 1, offsetSize);
    return readIntegerNonEmpty<ValueLength>(start() + end - offsetSize, offsetSize);
  }
  
  // return the number of members for an Object
  // must only be called for Slices that have been validated to be of type Object
  ValueLength objectLength() const {
    auto const h = head();
    VELOCYPACK_ASSERT(type(h) == ValueType::Object);

    if (h == 0x0a) {
      // special case: empty!
      return 0;
    }

    if (h == 0x14) {
      // compact Object
      ValueLength end = readVariableValueLength<false>(start() + 1);
      return readVariableValueLength<true>(start() + end - 1);
    }

    ValueLength const offsetSize = indexEntrySize(h);
    VELOCYPACK_ASSERT(offsetSize > 0);

    if (offsetSize < 8) {
      return readIntegerNonEmpty<ValueLength>(start() + offsetSize + 1, offsetSize);
    }

    ValueLength end = readIntegerNonEmpty<ValueLength>(start() + 1, offsetSize);
    return readIntegerNonEmpty<ValueLength>(start() + end - offsetSize, offsetSize);
  }

  // get the total byte size for a String slice, including the head byte
  // not check is done if the type of the slice is actually String 
  ValueLength stringSliceLength() const noexcept {
    // check if the type has a fixed length first
    auto const h = head();
    if (h == 0xbf) {
      // long UTF-8 String
      return static_cast<ValueLength>(
        1 + 8 + readIntegerFixed<ValueLength, 8>(start() + 1));
    }
    return static_cast<ValueLength>(1 + h - 0x40);
  }
  
  // translates an integer key into a string, without checks
  Slice translateUnchecked() const;

  Slice getFromCompactObject(StringRef const& attribute) const;

  // extract the nth member from an Array
  Slice getNth(ValueLength index) const;

  // extract the nth member from an Object, note that this is the nth
  // entry in the hash table for types 0x0b to 0x0e
  Slice getNthKey(ValueLength index, bool translate) const;

  // extract the nth member from an Object, no translation
  inline Slice getNthKeyUntranslated(ValueLength index) const {
    VELOCYPACK_ASSERT(type() == ValueType::Object);
    return Slice(start() + getNthOffset(index));
  }

  // get the offset for the nth member from a compact Array or Object type
  ValueLength getNthOffsetFromCompact(ValueLength index) const;
  
  // get the offset for the first member from a compact Array or Object type
  // it is only valid to call this method for compact Array or Object values with
  // at least one member!!
  ValueLength getStartOffsetFromCompact() const {
    VELOCYPACK_ASSERT(head() == 0x13 || head() == 0x14);

    ValueLength end = readVariableValueLength<false>(start() + 1);
    return 1 + getVariableValueLength(end);
  }

  constexpr inline ValueLength indexEntrySize(uint8_t head) const noexcept {
    return static_cast<ValueLength>(SliceStaticData::WidthMap[head]);
  }

  // perform a linear search for the specified attribute inside an Object
  Slice searchObjectKeyLinear(StringRef const& attribute, ValueLength ieBase,
                              ValueLength offsetSize, ValueLength n) const;

  // perform a binary search for the specified attribute inside an Object
  template<ValueLength offsetSize>
  Slice searchObjectKeyBinary(StringRef const& attribute, ValueLength ieBase, ValueLength n) const;

  // extracts a pointer from the slice and converts it into a
  // built-in pointer type
  char const* extractPointer() const {
    union {
      char const* value;
      char binary[sizeof(char const*)];
    };
    memcpy(&binary[0], start() + 1, sizeof(char const*));
    return value;
  }

  constexpr uint8_t tagOffset(uint8_t const* start) const noexcept {
    return SliceStaticData::TypeMap[*start] == ValueType::Tagged ? (*start == 0xee ? 2 : (*start == 0xef ? 9 : /* error */ 0)) : 0;
  }

  uint8_t tagsOffset(uint8_t const* start) const noexcept {
    uint8_t ret = 0;

    while(SliceStaticData::TypeMap[*start] == ValueType::Tagged) {
      uint8_t offset = tagOffset(start);
      ret += offset;
      start += offset;
    }

    return ret;
  }

  // get the total byte size for the slice, including the head byte
  ValueLength byteSize(uint8_t const* start) const {
    uint8_t h = *start;

    // check if the type has a fixed length first
    ValueLength l = static_cast<ValueLength>(SliceStaticData::FixedTypeLengths[h]);
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
          return readVariableValueLength<false>(start + 1);
        }

        VELOCYPACK_ASSERT(h > 0x01 && h <= 0x0e && h != 0x0a);
        if (h >= sizeof(SliceStaticData::WidthMap) / sizeof(SliceStaticData::WidthMap[0])) {
          throw Exception(Exception::InternalError, "invalid Array/Object type");
        }
        return readIntegerNonEmpty<ValueLength>(start + 1,
                                                SliceStaticData::WidthMap[h]);
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
            1 + 8 + readIntegerFixed<ValueLength, 8>(start + 1));
      }

      case ValueType::Binary: {
        VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);
        return static_cast<ValueLength>(
            1 + h - 0xbf + readIntegerNonEmpty<ValueLength>(start + 1, h - 0xbf));
      }

      case ValueType::BCD: {
        if (h <= 0xcf) {
          // positive BCD
          VELOCYPACK_ASSERT(h >= 0xc8 && h < 0xcf);
          return static_cast<ValueLength>(
              1 + h - 0xc7 + readIntegerNonEmpty<ValueLength>(start + 1, h - 0xc7));
        }

        // negative BCD
        VELOCYPACK_ASSERT(h >= 0xd0 && h < 0xd7);
        return static_cast<ValueLength>(
            1 + h - 0xcf + readIntegerNonEmpty<ValueLength>(start + 1, h - 0xcf));
      }

      case ValueType::Tagged: {
        uint8_t offset = tagsOffset(start);
        return byteSize(start + offset) + offset;
      }

      case ValueType::Custom: {
        VELOCYPACK_ASSERT(h >= 0xf4);
        switch (h) {
          case 0xf4:
          case 0xf5:
          case 0xf6: {
            return 2 + readIntegerFixed<ValueLength, 1>(start + 1);
          }

          case 0xf7:
          case 0xf8:
          case 0xf9:  {
            return 3 + readIntegerFixed<ValueLength, 2>(start + 1);
          }

          case 0xfa:
          case 0xfb:
          case 0xfc: {
            return 5 + readIntegerFixed<ValueLength, 4>(start + 1);
          }

          case 0xfd:
          case 0xfe:
          case 0xff: {
            return 9 + readIntegerFixed<ValueLength, 8>(start + 1);
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

    throw Exception(Exception::InternalError, "Invalid type for byteSize()");
  }

};

static_assert(!std::is_polymorphic<Slice>::value, "Slice must not be polymorphic");
static_assert(!std::has_virtual_destructor<Slice>::value, "Slice must not have virtual dtor");

}  // namespace arangodb::velocypack
}  // namespace arangodb

namespace std {
// implementation of std::hash for a Slice object
template <>
struct hash<arangodb::velocypack::Slice> {
  std::size_t operator()(arangodb::velocypack::Slice const& slice) const {
#ifdef VELOCYPACK_32BIT
    return static_cast<size_t>(slice.hash32());
#else
    return static_cast<std::size_t>(slice.hash());
#endif
  }
};

}

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Slice const*);

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Slice const&);

#endif

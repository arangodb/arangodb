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
#include <type_traits>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"
#include "velocypack/Options.h"
#include "velocypack/Value.h"
#include "velocypack/ValueType.h"

namespace arangodb {
namespace velocypack {

// forward for fasthash64 function declared elsewhere
uint64_t fasthash64(void const*, size_t, uint64_t);

class SliceScope;

class Slice {
  // This class provides read only access to a VPack value, it is
  // intentionally light-weight (only one pointer value), such that
  // it can easily be used to traverse larger VPack values.

  friend class Builder;

  uint8_t const* _start;

 public:
  Options const* options;

  // constructor for an empty Value of type None
  Slice() : Slice("\x00", &Options::Defaults) {}

  explicit Slice(uint8_t const* start,
                 Options const* options = &Options::Defaults)
      : _start(start), options(options) {
    VELOCYPACK_ASSERT(options != nullptr);
  }

  explicit Slice(char const* start, Options const* options = &Options::Defaults)
      : _start(reinterpret_cast<uint8_t const*>(start)), options(options) {
    VELOCYPACK_ASSERT(options != nullptr);
  }

  Slice(Slice const& other) : _start(other._start), options(other.options) {
    VELOCYPACK_ASSERT(options != nullptr);
  }

  Slice(Slice&& other) : _start(other._start), options(other.options) {
    VELOCYPACK_ASSERT(options != nullptr);
  }

  Slice& operator=(Slice const& other) {
    _start = other._start;
    options = other.options;
    VELOCYPACK_ASSERT(options != nullptr);
    return *this;
  }

  Slice& operator=(Slice&& other) {
    _start = other._start;
    options = other.options;
    VELOCYPACK_ASSERT(options != nullptr);
    return *this;
  }

  // creates a Slice from Json and adds it to a scope
  static Slice fromJson(SliceScope& scope, std::string const& json,
                        Options const* options = &Options::Defaults);

  uint8_t const* begin() { return _start; }

  uint8_t const* begin() const { return _start; }

  uint8_t const* end() { return _start + byteSize(); }

  uint8_t const* end() const { return _start + byteSize(); }

  // No destructor, does not take part in memory management,

  // get the type for the slice
  inline ValueType type() const { return TypeMap[head()]; }

  char const* typeName() const { return valueTypeName(type()); }

  // pointer to the head byte
  uint8_t const* start() const { return _start; }

  // pointer to the head byte
  template <typename T>
  T const* startAs() const {
    return reinterpret_cast<T const*>(_start);
  }

  // value of the head byte
  inline uint8_t head() const { return *_start; }

  inline uint64_t hash() const {
    return fasthash64(start(), checkOverflow(byteSize()), 0xdeadbeef);
  }

  // check if slice is of the specified type
  inline bool isType(ValueType t) const { return type() == t; }

  // check if slice is a None object
  bool isNone() const { return isType(ValueType::None); }

  // check if slice is a Null object
  bool isNull() const { return isType(ValueType::Null); }

  // check if slice is a Bool object
  bool isBool() const { return isType(ValueType::Bool); }

  // check if slice is a Bool object - this is an alias for isBool()
  bool isBoolean() const { return isBool(); }

  // check if slice is the Boolean value true
  bool isTrue() const { return head() == 0x1a; }

  // check if slice is the Boolean value false
  bool isFalse() const { return head() == 0x19; }

  // check if slice is an Array object
  bool isArray() const { return isType(ValueType::Array); }

  // check if slice is an Object object
  bool isObject() const { return isType(ValueType::Object); }

  // check if slice is a Double object
  bool isDouble() const { return isType(ValueType::Double); }

  // check if slice is a UTCDate object
  bool isUTCDate() const { return isType(ValueType::UTCDate); }

  // check if slice is an External object
  bool isExternal() const { return isType(ValueType::External); }

  // check if slice is a MinKey object
  bool isMinKey() const { return isType(ValueType::MinKey); }

  // check if slice is a MaxKey object
  bool isMaxKey() const { return isType(ValueType::MaxKey); }

  // check if slice is an Int object
  bool isInt() const { return isType(ValueType::Int); }

  // check if slice is a UInt object
  bool isUInt() const { return isType(ValueType::UInt); }

  // check if slice is a SmallInt object
  bool isSmallInt() const { return isType(ValueType::SmallInt); }

  // check if slice is a String object
  bool isString() const { return isType(ValueType::String); }

  // check if slice is a Binary object
  bool isBinary() const { return isType(ValueType::Binary); }

  // check if slice is a BCD
  bool isBCD() const { return isType(ValueType::BCD); }

  // check if slice is a Custom type
  bool isCustom() const { return isType(ValueType::Custom); }

  // check if a slice is any number type
  bool isInteger() const {
    return isType(ValueType::Int) || isType(ValueType::UInt) ||
           isType(ValueType::SmallInt);
  }

  // check if slice is any Number-type object
  bool isNumber() const { return isInteger() || isDouble(); }

  bool isSorted() const {
    auto const h = head();
    return (h >= 0x0b && h <= 0x0e);
  }

  // return the value for a Bool object
  bool getBool() const {
    if (type() != ValueType::Bool) {
      throw Exception(Exception::InvalidValueType, "Expecting type Bool");
    }
    return (head() == 0x1a);  // 0x19 == false, 0x1a == true
  }

  // return the value for a Bool object - this is an alias for getBool()
  bool getBoolean() const { return getBool(); }

  // return the value for a Double object
  double getDouble() const {
    if (type() != ValueType::Double) {
      throw Exception(Exception::InvalidValueType, "Expecting type Double");
    }
    union {
      uint64_t dv;
      double d;
    } v;
    v.dv = readInteger<uint64_t>(_start + 1, 8);
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
    if (!isType(ValueType::Array)) {
      throw Exception(Exception::InvalidValueType, "Expecting type Array");
    }

    return getNth(index);
  }

  Slice operator[](ValueLength index) const { return at(index); }

  // return the number of members for an Array or Object object
  ValueLength length() const {
    if (type() != ValueType::Array && type() != ValueType::Object) {
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
    ValueLength end = readInteger<ValueLength>(_start + 1, offsetSize);

    // find number of items
    if (h <= 0x05) {  // No offset table or length, need to compute:
      ValueLength firstSubOffset = findDataOffset(h);
      Slice first(_start + firstSubOffset, options);
      return (end - firstSubOffset) / first.byteSize();
    } else if (offsetSize < 8) {
      return readInteger<ValueLength>(_start + offsetSize + 1, offsetSize);
    }

    return readInteger<ValueLength>(_start + end - offsetSize, offsetSize);
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
  Slice keyAt(ValueLength index) const {
    if (!isType(ValueType::Object)) {
      throw Exception(Exception::InvalidValueType, "Expecting type Object");
    }

    return getNthKey(index, true);
  }

  Slice valueAt(ValueLength index) const {
    if (!isType(ValueType::Object)) {
      throw Exception(Exception::InvalidValueType, "Expecting type Object");
    }

    Slice key = getNthKey(index, false);
    return Slice(key.start() + key.byteSize(), options);
  }

  // look for the specified attribute path inside an Object
  // returns a Slice(ValueType::None) if not found
  Slice get(std::vector<std::string> const& attributes) const {
    size_t const n = attributes.size();
    if (n == 0) {
      throw Exception(Exception::InvalidAttributePath);
    }

    // use ourselves as the starting point
    Slice last = Slice(start(), options);
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
    if (type() != ValueType::External) {
      throw Exception(Exception::InvalidValueType, "Expecting type External");
    }
    return extractValue<char const*>();
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
          if (v < static_cast<double>(std::numeric_limits<T>::min()) ||
              v > static_cast<double>(std::numeric_limits<T>::max())) {
            throw Exception(Exception::NumberOutOfRange);
          }
          return static_cast<T>(v);
        }

        int64_t v = getInt();
        if (v < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
            v > static_cast<int64_t>(std::numeric_limits<T>::max())) {
          throw Exception(Exception::NumberOutOfRange);
        }
        return static_cast<T>(v);
      } else {
        // unsigned integral type
        if (isDouble()) {
          auto v = getDouble();
          if (v < 0.0 || v > static_cast<double>(UINT64_MAX) ||
              v > static_cast<double>(std::numeric_limits<T>::max())) {
            throw Exception(Exception::NumberOutOfRange);
          }
          return static_cast<T>(v);
        }

        uint64_t v = getUInt();
        if (v > static_cast<uint64_t>(std::numeric_limits<T>::max())) {
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
      return static_cast<T>(getInt());
    }
    if (isUInt()) {
      return static_cast<T>(getUInt());
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
    if (type() != ValueType::UTCDate) {
      throw Exception(Exception::InvalidValueType, "Expecting type UTCDate");
    }
    assertType(ValueType::UTCDate);
    uint64_t v = readInteger<uint64_t>(_start + 1, sizeof(uint64_t));
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
      length = readInteger<ValueLength>(_start + 1, 8);
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
      return readInteger<ValueLength>(_start + 1, 8);
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
      ValueLength length = readInteger<ValueLength>(_start + 1, 8);
      return std::string(reinterpret_cast<char const*>(_start + 1 + 8),
                         checkOverflow(length));
    }

    throw Exception(Exception::InvalidValueType, "Expecting type String");
  }

  // return the value for a Binary object
  uint8_t const* getBinary(ValueLength& length) const {
    if (type() != ValueType::Binary) {
      throw Exception(Exception::InvalidValueType, "Expecting type Binary");
    }

    uint8_t const h = head();
    VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);

    length = readInteger<ValueLength>(_start + 1, h - 0xbf);
    checkOverflow(length);
    return _start + 1 + h - 0xbf;
  }

  // return the length of the Binary slice
  ValueLength getBinaryLength() const {
    if (type() != ValueType::Binary) {
      throw Exception(Exception::InvalidValueType, "Expecting type Binary");
    }

    uint8_t const h = head();
    VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);

    return readInteger<ValueLength>(_start + 1, h - 0xbf);
  }

  // return a copy of the value for a Binary object
  std::vector<uint8_t> copyBinary() const {
    if (type() != ValueType::Binary) {
      throw Exception(Exception::InvalidValueType, "Expecting type Binary");
    }

    uint8_t const h = head();
    VELOCYPACK_ASSERT(h >= 0xc0 && h <= 0xc7);

    std::vector<uint8_t> out;
    ValueLength length = readInteger<ValueLength>(_start + 1, h - 0xbf);
    checkOverflow(length);
    out.reserve(static_cast<size_t>(length));
    out.insert(out.end(), _start + 1 + h - 0xbf,
               _start + 1 + h - 0xbf + length);
    return std::move(out);
  }

  // get the total byte size for the slice, including the head byte
  ValueLength byteSize() const {
    switch (type()) {
      case ValueType::None:
      case ValueType::Null:
      case ValueType::Bool:
      case ValueType::MinKey:
      case ValueType::MaxKey:
      case ValueType::SmallInt: {
        return 1;
      }

      case ValueType::Double: {
        return 1 + sizeof(double);
      }

      case ValueType::Array:
      case ValueType::Object: {
        auto const h = head();
        if (h == 0x01 || h == 0x0a) {
          // empty Array or Object
          return 1;
        }

        if (h == 0x13 || h == 0x14) {
          // compact Array or Object
          return readVariableValueLength<false>(_start + 1);
        }

        VELOCYPACK_ASSERT(h <= 0x12);
        return readInteger<ValueLength>(_start + 1, WidthMap[h]);
      }

      case ValueType::External: {
        return 1 + sizeof(char*);
      }

      case ValueType::UTCDate: {
        return 1 + sizeof(int64_t);
      }

      case ValueType::Int: {
        return static_cast<ValueLength>(1 + (head() - 0x1f));
      }

      case ValueType::UInt: {
        return static_cast<ValueLength>(1 + (head() - 0x27));
      }

      case ValueType::String: {
        auto const h = head();
        if (h == 0xbf) {
          // long UTF-8 String
          return static_cast<ValueLength>(
              1 + 8 + readInteger<ValueLength>(_start + 1, 8));
        }

        // short UTF-8 String
        return static_cast<ValueLength>(1 + h - 0x40);
      }

      case ValueType::Binary: {
        auto const h = head();
        return static_cast<ValueLength>(
            1 + h - 0xbf + readInteger<ValueLength>(_start + 1, h - 0xbf));
      }

      case ValueType::BCD: {
        auto const h = head();
        if (h <= 0xcf) {
          // positive BCD
          return static_cast<ValueLength>(
              1 + h - 0xc7 + readInteger<ValueLength>(_start + 1, h - 0xc7));
        }

        // negative BCD
        return static_cast<ValueLength>(
            1 + h - 0xcf + readInteger<ValueLength>(_start + 1, h - 0xcf));
      }

      case ValueType::Custom: {
        if (options->customTypeHandler == nullptr) {
          throw Exception(Exception::NeedCustomTypeHandler);
        }

        return options->customTypeHandler->byteSize(*this);
      }
    }

    throw Exception(Exception::InternalError);
  }

  Slice makeKey() const;

  int compareString(std::string const& attribute) const;
  bool isEqualString(std::string const& attribute) const;

  // check if two Slices are equal on the binary level
  bool equals(Slice const& other) const;

  std::string toJson() const;
  std::string toString() const;
  std::string hexType() const;

 private:
  Slice getFromCompactObject(std::string const& attribute) const;

  ValueLength findDataOffset(uint8_t head) const {
    // Must be called for a nonempty array or object at start():
    VELOCYPACK_ASSERT(head <= 0x12);
    unsigned int fsm = FirstSubMap[head];
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

  // extract the nth member from an Array
  Slice getNth(ValueLength index) const;

  // extract the nth member from an Object
  Slice getNthKey(ValueLength index, bool) const;

  // get the offset for the nth member from a compact Array or Object type
  ValueLength getNthOffsetFromCompact(ValueLength index) const;

  ValueLength indexEntrySize(uint8_t head) const {
    VELOCYPACK_ASSERT(head <= 0x12);
    return static_cast<ValueLength>(WidthMap[head]);
  }

  // perform a linear search for the specified attribute inside an Object
  Slice searchObjectKeyLinear(std::string const& attribute, ValueLength ieBase,
                              ValueLength offsetSize, ValueLength n) const;

  // perform a binary search for the specified attribute inside an Object
  Slice searchObjectKeyBinary(std::string const& attribute, ValueLength ieBase,
                              ValueLength offsetSize, ValueLength n) const;

// assert that the slice is of a specific type
// can be used for debugging and removed in production
#ifdef VELOCYPACK_ASSERT
  inline void assertType(ValueType) const {}
#else
  inline void assertType(ValueType type) const {
    VELOCYPACK_ASSERT(this->type() == type);
  }
#endif

  // extracts a value from the slice and converts it into a
  // built-in type
  template <typename T>
  T extractValue() const {
    union {
      T value;
      char binary[sizeof(T)];
    };
    memcpy(&binary[0], _start + 1, sizeof(T));
    return value;
  }

 private:
  static ValueType const TypeMap[256];
  static unsigned int const WidthMap[32];
  static unsigned int const FirstSubMap[32];
};

// a class for keeping Slice allocations in scope
class SliceScope {
 public:
  SliceScope(SliceScope const&) = delete;
  SliceScope& operator=(SliceScope const&) = delete;
  SliceScope();
  ~SliceScope();

  Slice add(uint8_t const* data, ValueLength size,
            Options const* options = &Options::Defaults);

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

// implementation of std::equal_to for two Slice objects
template <>
struct equal_to<arangodb::velocypack::Slice> {
  bool operator()(arangodb::velocypack::Slice const& left,
                  arangodb::velocypack::Slice const& right) const {
    return left.equals(right);
  }
};
}

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Slice const*);

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Slice const&);

#endif

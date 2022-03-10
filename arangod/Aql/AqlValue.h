////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Aql/AqlValueFwd.h"
#include "Aql/types.h"
#include "Basics/Endian.h"
#include "IResearch/Misc.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-common.h>
#include <string_view>
#include <vector>

namespace v8 {
template<class T>
class Local;
template<class T>
using Handle = Local<T>;
class Value;
class Isolate;
}  // namespace v8
namespace arangodb {
namespace aql {
class SharedAqlItemBlockPtr;
struct Range;
}  // namespace aql
namespace velocypack {
template<typename T>
class Buffer;
class Builder;
struct Options;
}  // namespace velocypack
}  // namespace arangodb

namespace arangodb {

class CollectionNameResolver;

namespace transaction {
class Methods;
}

namespace aql {
class AqlItemBlock;

// no-op struct used only internally to indicate that we want
// to copy the data behind the slice
struct AqlValueHintSliceCopy {
  explicit AqlValueHintSliceCopy(arangodb::velocypack::Slice s) noexcept;
  arangodb::velocypack::Slice slice;
};

// no-op struct used only internally to indicate that we want
// to NOT copy the slice data
struct AqlValueHintSliceNoCopy {
  explicit AqlValueHintSliceNoCopy(arangodb::velocypack::Slice s) noexcept;
  arangodb::velocypack::Slice slice;
};

struct AqlValueHintNone {
  constexpr AqlValueHintNone() noexcept = default;
};

struct AqlValueHintNull {
  constexpr AqlValueHintNull() noexcept = default;
};

struct AqlValueHintBool {
  explicit AqlValueHintBool(bool v) noexcept;
  bool const value;
};

struct AqlValueHintZero {
  constexpr AqlValueHintZero() noexcept = default;
};

struct AqlValueHintDouble {
  explicit AqlValueHintDouble(double v) noexcept;
  double const value;
};

struct AqlValueHintInt {
  explicit AqlValueHintInt(int64_t v) noexcept;
  explicit AqlValueHintInt(int v) noexcept;
  int64_t const value;
};

struct AqlValueHintUInt {
  explicit AqlValueHintUInt(uint64_t v) noexcept;
  uint64_t const value;
};

struct AqlValueHintEmptyArray {
  constexpr AqlValueHintEmptyArray() noexcept = default;
};

struct AqlValueHintEmptyObject {
  constexpr AqlValueHintEmptyObject() noexcept = default;
};

struct AqlValue final {
  friend struct std::hash<arangodb::aql::AqlValue>;
  friend struct std::equal_to<arangodb::aql::AqlValue>;

 public:
  /// @brief AqlValueType, indicates what sort of value we have
  enum AqlValueType : uint8_t {
    VPACK_INLINE = 0,     // contains vpack data, inline
    VPACK_SLICE_POINTER,  // contains a pointer to a vpack document, memory is
                          // not managed!
    VPACK_MANAGED_SLICE,  // contains vpack, via pointer to a managed uint8_t
                          // slice, allocated by new[] or malloc()
    RANGE,  // a pointer to a range remembering lower and upper bound, managed
    VPACK_INLINE_INT48,   // contains vpack data, inline and unpacked 48bit
                          // stored as 64bit int number value (system endianess)
    VPACK_INLINE_INT64,   // contains vpack data, inline and unpacked 64bit int
                          // number value (in little-endian)
    VPACK_INLINE_UINT64,  // contains vpack data, inline and unpacked 64bit uint
                          // number value (in little-endian)
    VPACK_INLINE_DOUBLE   // contains vpack data, inline and unpacked 64bit
                          // double number value (in little-endian)
  };

  static_assert(iresearch::adjacencyChecker<AqlValueType>::checkAdjacency<
                    VPACK_INLINE_DOUBLE, VPACK_INLINE_UINT64,
                    VPACK_INLINE_INT64, VPACK_INLINE_INT48>(),
                "Values are not adjacent");

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
  /// data and is managed by the AqlValue. The second-last byte contains info
  /// about how the memory was allocated:
  /// - MemoryOriginType::New: memory was allocated by new[] and must be deleted
  /// - MemoryOriginType::Malloc: memory was malloc'd and needs to be free'd
  /// RANGE: a managed range object. The memory is managed by the AqlValue
  ///
  /// AqlValue memory layout:
  /// Legend:
  /// AT - AqlValue Type byte
  /// ST - slice type byte
  /// SD - slice data  (always little-endian for numbers)
  /// ND - number data (in machine endianess)
  /// PD - pointer data
  /// MO - memory origin
  /// ML - managed slice length
  /// ID - isDocument flag
  /// XX - unused
  /// | 0  | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10 | 11 | 12 | 13 | 14
  /// | 15 |   Bytes | AT | ID | XX | XX | XX | XX | XX | XX | PD | PD | PD | PD
  /// | PD | PD | PD | PD |   VPACK_SLICE_POINTER | AT | MO | ML | ML | ML | ML
  /// | ML | ML | PD | PD | PD | PD | PD | PD | PD | PD |   VPACK_MANAGED_SLICE
  /// | AT | XX | XX | XX | XX | XX | XX | XX | PD | PD | PD | PD | PD | PD | PD
  /// | PD |   RANGE | AT | ST | SD | SD | SD | SD | SD | SD | SD | SD | SD | SD
  /// | SD | SD | SD | SD |   VPACK_INLINE | AT | ST | SD | SD | SD | SD | SD |
  /// SD | ND | ND | ND | ND | ND | ND | ND | ND |   VPACK_48BIT_INLINE_INT | AT
  /// | XX | XX | XX | XX | XX | XX | ST | SD | SD | SD | SD | SD | SD | SD | SD
  /// |   VPACK_64BIT_INLINE_(INT/UINT/DOUBLE)
 private:
  union {
    uint8_t aqlValueType;
    uint64_t words[2];  // keep this for fast zeroing AqlValue

    // RANGE
    struct {
      uint8_t padding[8];
      Range const* range;
    } rangeMeta;
    static_assert(sizeof(rangeMeta) == 16, "RANGE layout is not 16 bytes!");

    // VPACK_MANAGED_SLICE
    struct {
      uint64_t getLength() const noexcept {
        if constexpr (basics::isLittleEndian()) {
          return (lengthOrigin & 0xffffffffffff0000ULL) >> 16;
        } else {
          return (lengthOrigin & 0x0000ffffffffffffULL);
        }
      }

      uint64_t getOrigin() const noexcept {
        if constexpr (basics::isLittleEndian()) {
          return (lengthOrigin & 0x000000000000ff00ULL) >> 8;
        } else {
          return (lengthOrigin & 0x00ff000000000000ULL) >> 48;
        }
      }

      uint64_t lengthOrigin;  // First byte is AqlValue type. Second -  Memory
                              // origin. Other 6 bytes  - length
      uint8_t* managedPointer;
    } managedSliceMeta;
    static_assert(sizeof(managedSliceMeta) == 16,
                  "VPACK_MANAGED_SLICE layout is not 16 bytes!");

    // VPACK_SLICE_POINTER
    struct {
      uint8_t padding;
      uint8_t isManagedDoc;
      uint8_t padding2[6];
      uint8_t const* pointer;
    } pointerMeta;
    static_assert(sizeof(pointerMeta) == 16,
                  "VPACK_SLICE_POINTER layout is not 16 bytes!");

    // VPACK_INLINE
    struct {
      uint8_t padding;
      uint8_t slice[15];
    } inlineSliceMeta;

    // VPACK_INLINE_INT48
    struct {
      union {
        struct {
          uint8_t padding;
          uint8_t slice[7];
        } slice;
        struct {
          uint8_t padding[8];
          int64_t val;
        } int48;
      } data;
    } shortNumberMeta;
    static_assert(sizeof(shortNumberMeta) == 16,
                  "VPACK_INLINE_INT48 layout is not 16 bytes!");

    // VPACK_INLINE_INT64
    // VPACK_INLINE_UINT64
    // VPACK_INLINE_DOUBLE
    struct {
      union {
        struct {
          uint8_t padding[7];
          uint8_t slice[9];
        } slice;
        struct {
          uint8_t padding[8];
          uint64_t val;
        } uintLittleEndian;  // also stores double
        struct {
          uint8_t padding[8];
          int64_t val;
        } intLittleEndian;
      } data;
    } longNumberMeta;
    static_assert(sizeof(longNumberMeta) == 16,
                  "VPACK_INLINE_INT64 layout is not 16 bytes!");
  } _data;

  /// @brief type of memory that we are dealing with for values of type
  /// VPACK_MANAGED_SLICE
  enum class MemoryOriginType : uint8_t {
    New = 0,     // memory allocated by new[]
    Malloc = 1,  // memory allocated by malloc
  };

 public:
  // construct an empty AqlValue
  // note: this is the default constructor and should be as cheap as possible
  AqlValue() noexcept;

  // construct from pointer, not copying!
  explicit AqlValue(uint8_t const* pointer) noexcept;

  // construct from another AqlValue and a new data pointer, not copying!
  explicit AqlValue(AqlValue const& other, void* data) noexcept;

  explicit AqlValue(AqlValueHintNone) noexcept;

  explicit AqlValue(AqlValueHintNull) noexcept;

  explicit AqlValue(AqlValueHintBool v) noexcept;

  explicit AqlValue(AqlValueHintZero) noexcept;

  // construct from a double value
  explicit AqlValue(AqlValueHintDouble v) noexcept;

  // construct from an int64 value
  explicit AqlValue(AqlValueHintInt v) noexcept;

  // construct from a uint64 value
  explicit AqlValue(AqlValueHintUInt v) noexcept;

  // construct from char* and length, copying the string
  [[deprecated]] AqlValue(char const* value, size_t length);

  // construct from std::string
  explicit AqlValue(std::string_view value);

  explicit AqlValue(AqlValueHintEmptyArray) noexcept;

  explicit AqlValue(AqlValueHintEmptyObject) noexcept;

  // construct from Buffer, potentially taking over its ownership
  explicit AqlValue(arangodb::velocypack::Buffer<uint8_t>&& buffer);

  // construct from slice data, not copying!
  explicit AqlValue(AqlValueHintSliceNoCopy v) noexcept;

  // construct from slice data, copying the data
  explicit AqlValue(AqlValueHintSliceCopy v);

  // construct from Slice, copying contents
  explicit AqlValue(arangodb::velocypack::Slice slice);

  // construct from Slice and length, copying contents
  AqlValue(arangodb::velocypack::Slice slice,
           arangodb::velocypack::ValueLength length);

  // construct range type
  AqlValue(int64_t low, int64_t high);

  /// @brief AqlValues can be copied and moved as required
  /// memory management is not performed via AqlValue destructor but via
  /// explicit calls to destroy()
  AqlValue(AqlValue const&) noexcept = default;
  AqlValue& operator=(AqlValue const&) noexcept = default;
  AqlValue(AqlValue&&) noexcept = default;
  AqlValue& operator=(AqlValue&&) noexcept = default;

  ~AqlValue() noexcept = default;

  /// @brief whether or not the value must be destroyed
  bool requiresDestruction() const noexcept;

  /// @brief whether or not the value is empty / none
  bool isEmpty() const noexcept;

  /// @brief whether or not the value is a pointer
  bool isPointer() const noexcept;

  /// @brief whether or not the value is an external manager document
  bool isManagedDocument() const noexcept;

  /// @brief whether or not the value is a range
  bool isRange() const noexcept;

  /// @brief hashes the value
  uint64_t hash(uint64_t seed = 0xdeadbeef) const;

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

  // @brief return a string describing the content of this AqlValue
  char const* getTypeString() const noexcept;

  /// @brief get the (array) length (note: this treats ranges as arrays, too!)
  size_t length() const;

  /// @brief get the (array) element at position
  AqlValue at(int64_t position, bool& mustDestroy, bool copy) const;

  /// @brief get the (array) element at position
  AqlValue at(int64_t position, size_t n, bool& mustDestroy, bool copy) const;

  /// @brief get the _key attribute from an object/document
  AqlValue getKeyAttribute(bool& mustDestroy, bool copy) const;
  /// @brief get the _id attribute from an object/document
  AqlValue getIdAttribute(CollectionNameResolver const& resolver,
                          bool& mustDestroy, bool copy) const;
  /// @brief get the _from attribute from an object/document
  AqlValue getFromAttribute(bool& mustDestroy, bool copy) const;
  /// @brief get the _to attribute from an object/document
  AqlValue getToAttribute(bool& mustDestroy, bool copy) const;

  /// @brief get the (object) element by name(s)
  AqlValue get(CollectionNameResolver const& resolver, std::string_view name,
               bool& mustDestroy, bool copy) const;
  AqlValue get(CollectionNameResolver const& resolver,
               std::vector<std::string> const& names, bool& mustDestroy,
               bool copy) const;
  bool hasKey(std::string_view name) const;

  /// @brief get the numeric value of an AqlValue
  double toDouble() const;
  double toDouble(bool& failed) const;
  int64_t toInt64() const;

  /// @brief whether or not an AqlValue evaluates to true/false
  bool toBoolean() const;

  /// @brief return the pointer to the underlying AqlValue.
  /// only supported for AqlValue types with dynamic memory management
  void* data() const noexcept;

  /// @brief return the range value
  Range const* range() const;

  /// @brief construct a V8 value as input for the expression execution in V8
  v8::Handle<v8::Value> toV8(v8::Isolate* isolate,
                             arangodb::velocypack::Options const*) const;

  /// @brief materializes a value into the builder
  void toVelocyPack(velocypack::Options const*, arangodb::velocypack::Builder&,
                    bool resolveExternals, bool allowUnindexed) const;

  /// @brief materialize a value into a new one. this expands ranges
  AqlValue materialize(velocypack::Options const*, bool& hasCopied,
                       bool resolveExternals) const;

  /// @brief return the slice for the value
  /// this will throw if the value type is not VPACK_SLICE_POINTER,
  /// VPACK_INLINE, VPACK_MANAGED_SLICE
  arangodb::velocypack::Slice slice() const;

  arangodb::velocypack::Slice slice(AqlValueType type) const;

  /// @brief clone a value
  AqlValue clone() const;

  /// @brief invalidates/resets a value to None, not freeing any memory
  void erase() noexcept;

  /// @brief destroy, explicit destruction, only when needed
  void destroy() noexcept;

  /// @brief returns the size of the dynamic memory allocated for the value
  size_t memoryUsage() const noexcept;

  /// @brief compare function for two values
  static int Compare(velocypack::Options const*, AqlValue const& left,
                     AqlValue const& right, bool useUtf8);

  /// @brief Returns the type of this value. If true it uses an external pointer
  /// if false it uses the internal data structure.
  /// Do not move it to cpp file as MSVC does not inline it.
  AqlValueType type() const noexcept {
    return static_cast<AqlValueType>(_data.aqlValueType);
  }

 private:
  /// @brief initializes value from a slice
  void initFromSlice(arangodb::velocypack::Slice slice);

  /// @brief initializes value from a slice, when the length is already known
  void initFromSlice(arangodb::velocypack::Slice slice,
                     arangodb::velocypack::ValueLength length);

  /// @brief sets the value type
  void setType(AqlValueType type) noexcept;

  template<bool isManagedDoc>
  void setPointer(uint8_t const* pointer) noexcept;

  /// @brief return the memory origin type for values of type
  /// VPACK_MANAGED_SLICE
  MemoryOriginType memoryOriginType() const noexcept;

  /// @brief store meta information for values of type VPACK_MANAGED_SLICE
  void setManagedSliceData(MemoryOriginType mot,
                           arangodb::velocypack::ValueLength length);
};

// Check that the defaulted constructors, destructor and assignment
// operators are all noexcept:
// AqlValue(AqlValue&&)
static_assert(noexcept(AqlValue(std::declval<AqlValue>())));
// AqlValue(AqlValue const&)
static_assert(
    noexcept(AqlValue(static_cast<AqlValue const&>(std::declval<AqlValue>()))));
// AqlValue& operator=(AqlValue&&)
static_assert(noexcept(std::declval<AqlValue>() = std::declval<AqlValue>()));
// AqlValue& operator=(AqlValue const&)
static_assert(noexcept(std::declval<AqlValue>() = static_cast<AqlValue const&>(
                           std::declval<AqlValue>())));
// ~AqlValue()
static_assert(noexcept(std::declval<AqlValue>().~AqlValue()));

class AqlValueGuard {
 public:
  AqlValueGuard() = delete;
  AqlValueGuard(AqlValueGuard const&) = delete;
  AqlValueGuard& operator=(AqlValueGuard const&) = delete;
  AqlValueGuard(AqlValueGuard&&) = delete;
  AqlValueGuard& operator=(AqlValueGuard&&) = delete;

  AqlValueGuard(AqlValue& value, bool destroy) noexcept;
  ~AqlValueGuard() noexcept;

  void steal() noexcept;
  AqlValue& value() noexcept;

 private:
  AqlValue& _value;
  bool _destroy;
};

static_assert(sizeof(AqlValue) == 16, "invalid AqlValue size");

}  // namespace aql

}  // namespace arangodb

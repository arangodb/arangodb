////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_VELOCY_PACK_HELPER_H
#define ARANGODB_IRESEARCH__IRESEARCH_VELOCY_PACK_HELPER_H 1

#include "Basics/Common.h"
#include "Basics/debugging.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "utils/string.hpp"  // for irs::string_ref

namespace arangodb {
namespace velocypack {

class Builder;  // forward declarations

}  // namespace velocypack
}  // namespace arangodb

namespace arangodb {
namespace iresearch {

// according to Slice.h:330
uint8_t const COMPACT_ARRAY = 0x13;
uint8_t const COMPACT_OBJECT = 0x14;

template<typename Char>
irs::basic_string_ref<Char> ref(VPackSlice slice) {
  static_assert(sizeof(Char) == sizeof(uint8_t),
                "sizeof(Char) != sizeof(uint8_t)");

  return irs::basic_string_ref<Char>(
    reinterpret_cast<Char const*>(slice.begin()),
    slice.byteSize());
}

template<typename Char>
VPackSlice slice(irs::basic_string_ref<Char> const& ref) {
  static_assert(sizeof(Char) == sizeof(uint8_t),
                "sizeof(Char) != sizeof(uint8_t)");

  return VPackSlice(reinterpret_cast<uint8_t const*>(ref.c_str()));
}

template<typename Char>
VPackSlice slice(std::basic_string<Char> const& ref) {
  static_assert(sizeof(Char) == sizeof(uint8_t),
                "sizeof(Char) != sizeof(uint8_t)");

  return VPackSlice(reinterpret_cast<uint8_t const*>(ref.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a string_ref value to the 'builder' (for JSON arrays)
////////////////////////////////////////////////////////////////////////////////
arangodb::velocypack::Builder& addBytesRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::bytes_ref const& value // value
);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a string_ref value to the 'builder' (for JSON objects)
////////////////////////////////////////////////////////////////////////////////
arangodb::velocypack::Builder& addBytesRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::string_ref const& key, // key
  irs::bytes_ref const& value // value
);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a string_ref value to the 'builder' (for JSON arrays)
////////////////////////////////////////////////////////////////////////////////
arangodb::velocypack::Builder& addStringRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::string_ref const& value // value
);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps bytes ref with VPackValuePair
////////////////////////////////////////////////////////////////////////////////
inline arangodb::velocypack::ValuePair toValuePair(irs::bytes_ref const& ref) {
  TRI_ASSERT(!ref.null()); // consumers of ValuePair usually use memcpy(...) which cannot handle nullptr
  return arangodb::velocypack::ValuePair( // value pair
    ref.c_str(), ref.size(), arangodb::velocypack::ValueType::Binary // args
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps string ref with VPackValuePair
////////////////////////////////////////////////////////////////////////////////
inline arangodb::velocypack::ValuePair toValuePair(irs::string_ref const& ref) {
  TRI_ASSERT(!ref.null()); // consumers of ValuePair usually use memcpy(...) which cannot handle nullptr
  return arangodb::velocypack::ValuePair( // value pair
    ref.c_str(), ref.size(), arangodb::velocypack::ValueType::String // args
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a string_ref value to the 'builder' (for JSON objects)
////////////////////////////////////////////////////////////////////////////////
arangodb::velocypack::Builder& addStringRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::string_ref const& key, // key
  irs::string_ref const& value // value
);

inline bool isArrayOrObject(VPackSlice const& slice) {
  auto const type = slice.type();
  return VPackValueType::Array == type || VPackValueType::Object == type;
}

inline bool isCompactArrayOrObject(VPackSlice const& slice) {
  TRI_ASSERT(isArrayOrObject(slice));

  auto const head = slice.head();
  return COMPACT_ARRAY == head || COMPACT_OBJECT == head;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief extracts string_ref from VPackSlice, note that provided 'slice'
///        must be a string
/// @return extracted string_ref
//////////////////////////////////////////////////////////////////////////////
inline irs::string_ref getStringRef(VPackSlice const& slice) {
  if (slice.isNull()) {
    return irs::string_ref::NIL;
  }

  TRI_ASSERT(slice.isString());

  arangodb::velocypack::ValueLength size;
  auto const* str = slice.getString(size);

  static_assert(sizeof(arangodb::velocypack::ValueLength) == sizeof(size_t),
                "sizeof(arangodb::velocypack::ValueLength) != sizeof(size_t)");

  return irs::string_ref(str, size);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief extracts string_ref from VPackSlice, note that provided 'slice'
///        must be a string
/// @return extracted string_ref
//////////////////////////////////////////////////////////////////////////////
inline irs::bytes_ref getBytesRef(VPackSlice const& slice) {
  TRI_ASSERT(slice.isString());

  arangodb::velocypack::ValueLength size;
  auto const* str = slice.getString(size);

  static_assert(sizeof(arangodb::velocypack::ValueLength) == sizeof(size_t),
                "sizeof(arangodb::velocypack::ValueLength) != sizeof(size_t)");

  return irs::bytes_ref(reinterpret_cast<irs::byte_type const*>(str), size);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief parses a numeric sub-element
/// @return success
//////////////////////////////////////////////////////////////////////////////
template <typename T>
inline bool getNumber(T& buf, arangodb::velocypack::Slice const& slice) noexcept {
  if (!slice.isNumber()) {
    return false;
  }

  typedef typename std::conditional<std::is_floating_point<T>::value, T, double>::type NumType;

  try {
    auto value = slice.getNumber<NumType>();

    buf = static_cast<T>(value);

    return value == static_cast<decltype(value)>(buf);
  } catch (...) {
    // NOOP
  }

  return false;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief parses a numeric sub-element, or uses a default if it does not exist
/// @return success
//////////////////////////////////////////////////////////////////////////////
template <typename T>
inline bool getNumber(T& buf, arangodb::velocypack::Slice const& slice,
                      std::string_view fieldName, bool& seen, T fallback) noexcept {
  seen = slice.hasKey(fieldName.data(), fieldName.length());

  if (!seen) {
    buf = fallback;

    return true;
  }

  return getNumber(buf, slice.get(fieldName));
}

//////////////////////////////////////////////////////////////////////////////
/// @brief parses a string sub-element, or uses a default if it does not exist
/// @return success
//////////////////////////////////////////////////////////////////////////////
inline bool getString(std::string& buf, arangodb::velocypack::Slice const& slice,
                      std::string_view fieldName, bool& seen,
                      std::string const& fallback) noexcept {
  seen = slice.hasKey(fieldName.data(), fieldName.length());

  if (!seen) {
    buf = fallback;

    return true;
  }

  auto field = slice.get(fieldName);

  if (!field.isString()) {
    return false;
  }

  buf = field.copyString();

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief parses a string sub-element, or uses a default if it does not exist
/// @return success
//////////////////////////////////////////////////////////////////////////////
inline bool getString(irs::string_ref& buf, arangodb::velocypack::Slice const& slice,
                      std::string_view fieldName, bool& seen,
                      irs::string_ref const& fallback) noexcept {
  seen = slice.hasKey(fieldName.data(), fieldName.length());

  if (!seen) {
    buf = fallback;

    return true;
  }

  auto field = slice.get(fieldName);

  if (!field.isString()) {
    return false;
  }

  buf = getStringRef(field);

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief look for the specified attribute path inside an Object
/// @return a value denoted by 'fallback' if not found
//////////////////////////////////////////////////////////////////////////////
template<typename T>
VPackSlice get(VPackSlice slice,
               const T& attributePath,
               VPackSlice fallback = VPackSlice::nullSlice()) {
  if (attributePath.empty()) {
    return fallback;
  }

  for (size_t i = 0, size = attributePath.size(); i < size; ++i) {
    slice = slice.get(attributePath[i].name);

    if (slice.isNone() || (i + 1 < size && !slice.isObject())) {
      return fallback;
    }
  }

  return slice;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief append the contents of the slice to the builder
/// @return success
//////////////////////////////////////////////////////////////////////////////
bool mergeSlice(arangodb::velocypack::Builder& builder,
                arangodb::velocypack::Slice const& slice);

//////////////////////////////////////////////////////////////////////////////
/// @brief append the contents of the slice to the builder skipping keys
/// @return success
//////////////////////////////////////////////////////////////////////////////
bool mergeSliceSkipKeys(arangodb::velocypack::Builder& builder,
                        arangodb::velocypack::Slice const& slice,
                        std::function<bool(irs::string_ref const& key)> const& acceptor);

//////////////////////////////////////////////////////////////////////////////
/// @brief append the contents of the slice to the builder skipping offsets
/// @return success
//////////////////////////////////////////////////////////////////////////////
bool mergeSliceSkipOffsets(arangodb::velocypack::Builder& builder,
                           arangodb::velocypack::Slice const& slice,
                           std::function<bool(size_t offset)> const& acceptor);

////////////////////////////////////////////////////////////////////////////
/// @struct IteratorValue
/// @brief represents of value of the iterator
////////////////////////////////////////////////////////////////////////////
struct IteratorValue {
  ///////////////////////////////////////////////////////////////////////////
  /// @brief type of the current level (Array or Object)
  ///////////////////////////////////////////////////////////////////////////
  VPackValueType type;

  ///////////////////////////////////////////////////////////////////////////
  /// @brief position at the current level
  ///////////////////////////////////////////////////////////////////////////
  VPackValueLength pos;

  ///////////////////////////////////////////////////////////////////////////
  /// @brief current key at the current level
  ///          type == Array --> key == value;
  ///////////////////////////////////////////////////////////////////////////
  VPackSlice key;

  ///////////////////////////////////////////////////////////////////////////
  /// @brief current value at the current level
  ///////////////////////////////////////////////////////////////////////////
  VPackSlice value;
};  // IteratorValue

class Iterator {
 public:
  explicit Iterator(VPackSlice slice);

  // returns true if iterator exhausted
  bool next() noexcept;
  bool valid() const noexcept { return 0 != _length; }

  IteratorValue const& value() const noexcept { return operator*(); }

  IteratorValue const& operator*() const noexcept { return _value; }

 private:
  VPackValueLength _length;
  uint8_t const* _begin;
  IteratorValue _value;
};  // Iterator

}  // namespace iresearch
}  // namespace arangodb

#endif

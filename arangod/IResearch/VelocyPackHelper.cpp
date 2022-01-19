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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

namespace {
template<typename T>
arangodb::velocypack::Builder& addRef(arangodb::velocypack::Builder& builder,
                                      irs::basic_string_ref<T> value) {
  // store nulls verbatim
  if (value.null()) {
    builder.add(  // add value
        arangodb::velocypack::Value(
            arangodb::velocypack::ValueType::Null)  // value
    );
  } else {
    builder.add(arangodb::iresearch::toValuePair(value));
  }

  return builder;
}

template<typename T>
arangodb::velocypack::Builder& addRef(arangodb::velocypack::Builder& builder,
                                      irs::string_ref key,
                                      irs::basic_string_ref<T> value) {
  // Builder uses memcpy(...) which cannot handle nullptr
  TRI_ASSERT(!key.null());

  // store nulls verbatim
  if (value.null()) {
    builder.add(
        key.c_str(), key.size(),
        arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null));
  } else {
    builder.add(key.c_str(), key.size(),
                arangodb::iresearch::toValuePair(value));
  }

  return builder;
}
}  // namespace

namespace arangodb {
namespace iresearch {

velocypack::Builder& addBytesRef(velocypack::Builder& builder,
                                 irs::bytes_ref value) {
  return addRef(builder, value);
}

velocypack::Builder& addBytesRef(velocypack::Builder& builder,
                                 irs::string_ref key, irs::bytes_ref value) {
  return addRef(builder, key, value);
}

velocypack::Builder& addStringRef(velocypack::Builder& builder,
                                  irs::string_ref value) {
  return addRef(builder, value);
}

velocypack::Builder& addStringRef(velocypack::Builder& builder,
                                  irs::string_ref key, irs::string_ref value) {
  return addRef(builder, key, value);
}

bool mergeSlice(velocypack::Builder& builder, velocypack::Slice slice) {
  if (builder.isOpenArray()) {
    if (slice.isArray()) {
      builder.add(velocypack::ArrayIterator(slice));
    } else {
      builder.add(slice);
    }

    return true;
  }

  if (builder.isOpenObject() && slice.isObject()) {
    builder.add(velocypack::ObjectIterator(slice));

    return true;
  }

  return false;
}

bool mergeSliceSkipKeys(
    velocypack::Builder& builder, velocypack::Slice slice,
    std::function<bool(irs::string_ref key)> const& acceptor) {
  if (!builder.isOpenObject() || !slice.isObject()) {
    return mergeSlice(builder, slice);  // no keys to skip for non-objects
  }

  for (velocypack::ObjectIterator itr(slice); itr.valid(); ++itr) {
    auto key = itr.key();
    auto value = itr.value();

    if (!key.isString()) {
      return false;
    }

    auto attr = getStringRef(key);

    if (acceptor(attr)) {
      builder.add(attr.c_str(), attr.size(), value);
    }
  }

  return true;
}

bool mergeSliceSkipOffsets(velocypack::Builder& builder,
                           velocypack::Slice slice,
                           std::function<bool(size_t offset)> const& acceptor) {
  if (!builder.isOpenArray() || !slice.isArray()) {
    return mergeSlice(builder, slice);  // no offsets to skip for non-arrays
  }

  for (velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
    if (acceptor(itr.index())) {
      builder.add(*itr);
    }
  }

  return true;
}

// can't make it noexcept since VPackSlice::getNthOffset is not noexcept
Iterator::Iterator(VPackSlice slice) {
  TRI_ASSERT(isArrayOrObject(slice));
  _length = slice.length();

  if (0 == _length) {
    return;
  }

  // according to Iterator.h:160 and Iterator.h:194
  auto const offset = isCompactArrayOrObject(slice)
                          ? slice.getNthOffset(0)
                          : slice.findDataOffset(slice.head());
  _begin = slice.start() + offset;

  _value.type = slice.type();

  static_assert(!std::numeric_limits<VPackValueLength>::is_signed);
  _value.pos = VPackValueLength(0) - 1;
}

bool Iterator::next() noexcept {
  if (0 == _length) {
    return false;
  }

  // whether or not we're in the context of array or object
  VPackValueLength const isObject = VPackValueType::Array != _value.type;

  _value.key = VPackSlice(_begin);
  _value.value = VPackSlice(_begin + isObject * _value.key.byteSize());

  _begin = _value.value.start() + _value.value.byteSize();
  ++_value.pos;
  --_length;

  return true;
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

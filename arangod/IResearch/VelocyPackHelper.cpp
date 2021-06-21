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

#include "VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

namespace {
template<typename T>
arangodb::velocypack::Builder& addRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::basic_string_ref<T> const& value // value
) {
  // store nulls verbatim
  if (value.null()) {
    builder.add( // add value
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null) // value
    );
  } else {
    builder.add(arangodb::iresearch::toValuePair(value));
  }

  return builder;
}

template<typename T>
arangodb::velocypack::Builder& addRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::string_ref const& key, // key
  irs::basic_string_ref<T> const& value // value
) {
  TRI_ASSERT(!key.null()); // Builder uses memcpy(...) which cannot handle nullptr

  // store nulls verbatim
  if (value.null()) {
    builder.add( // add value
      key.c_str(), // key data
      key.size(), // key size
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null) // value
    );
  } else {
    builder.add(key.c_str(), key.size(), arangodb::iresearch::toValuePair(value));
  }

  return builder;
}
}

namespace arangodb {
namespace iresearch {

arangodb::velocypack::Builder& addBytesRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::bytes_ref const& value // value
) {
  return addRef(builder, value);
}

arangodb::velocypack::Builder& addBytesRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::string_ref const& key, // key
  irs::bytes_ref const& value // value
) {
  return addRef(builder, key, value);
}

arangodb::velocypack::Builder& addStringRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::string_ref const& value // value
) {
  return addRef(builder, value);
}

arangodb::velocypack::Builder& addStringRef( // add a value
  arangodb::velocypack::Builder& builder, // builder
  irs::string_ref const& key, // key
  irs::string_ref const& value // value
) {
  return addRef(builder, key, value);
}

bool mergeSlice(arangodb::velocypack::Builder& builder,
                arangodb::velocypack::Slice const& slice) {
  if (builder.isOpenArray()) {
    if (slice.isArray()) {
      builder.add(arangodb::velocypack::ArrayIterator(slice));
    } else {
      builder.add(slice);
    }

    return true;
  }

  if (builder.isOpenObject() && slice.isObject()) {
    builder.add(arangodb::velocypack::ObjectIterator(slice));

    return true;
  }

  return false;
}

bool mergeSliceSkipKeys(arangodb::velocypack::Builder& builder,
                        arangodb::velocypack::Slice const& slice,
                        std::function<bool(irs::string_ref const& key)> const& acceptor) {
  if (!builder.isOpenObject() || !slice.isObject()) {
    return mergeSlice(builder, slice);  // no keys to skip for non-objects
  }

  for (arangodb::velocypack::ObjectIterator itr(slice); itr.valid(); ++itr) {
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

bool mergeSliceSkipOffsets(arangodb::velocypack::Builder& builder,
                           arangodb::velocypack::Slice const& slice,
                           std::function<bool(size_t offset)> const& acceptor) {
  if (!builder.isOpenArray() || !slice.isArray()) {
    return mergeSlice(builder, slice);  // no offsets to skip for non-arrays
  }

  for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
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

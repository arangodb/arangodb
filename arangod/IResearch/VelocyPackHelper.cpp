//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "VelocyPackHelper.h"

#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

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
void Iterator::reset() {
  TRI_ASSERT(isArrayOrObject(_slice));

  if (!_size) {
    return;
  }

  // according to Iterator.h:160 and Iterator.h:194
  auto const offset = isCompactArrayOrObject(_slice)
                          ? _slice.getNthOffset(0)
                          : _slice.findDataOffset(_slice.head());

  _value.reset(_slice.start() + offset);
}

bool Iterator::next() noexcept {
  if (++_value.pos < _size) {
    auto const& value = _value.value;
    _value.reset(value.start() + value.byteSize());
    return true;
  }
  return false;
}

ObjectIterator::ObjectIterator(VPackSlice const& slice) {
  if (isArrayOrObject(slice)) {
    _stack.emplace_back(slice);

    while (isArrayOrObject(topValue())) {
      _stack.emplace_back(topValue());
    }
  }
}

ObjectIterator& ObjectIterator::operator++() {
  TRI_ASSERT(valid());

  for (top().next(); !top().valid(); top().next()) {
    _stack.pop_back();

    if (!valid()) {
      return *this;
    }
  }

  while (isArrayOrObject(topValue())) {
    _stack.emplace_back(topValue());
  }

  return *this;
}

NS_END      // iresearch
    NS_END  // arangodb

    // -----------------------------------------------------------------------------
    // --SECTION-- END-OF-FILE
    // -----------------------------------------------------------------------------
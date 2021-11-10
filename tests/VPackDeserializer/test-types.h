////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#ifndef DESERIALIZER_TEST_TYPES_H
#define DESERIALIZER_TEST_TYPES_H
#include <iostream>
#include <memory>
#include <utility>
#include "velocypack/Buffer.h"
#include "velocypack/Iterator.h"
#include "velocypack/Slice.h"

namespace arangodb::tests::deserializer {

struct slice_access {
  enum class type {
    GET,
    HAS_KEY,
    COPY_STRING,
    IS_NUMBER,
    IS_ARRAY,
    IS_OBJECT,
    IS_NONE,
    LENGTH,
    AT,
    GET_NUMBER,
    IS_STRING,
    IS_BOOL,
    GET_BOOL,
    GET_NUMERIC_VALUE,
    IS_EQUAL_STRING,
    STRING_VIEW,
    ARRAY_ITER_ACCESS,
    OBJECT_ITER_ACCESS,
  };

  std::string key;
  std::string parameter;
  type what;

  slice_access(std::string key, type what) : key(std::move(key)), what(what) {}
  slice_access(std::string key, type what, std::string parameter)
    : key(std::move(key)), parameter(std::move(parameter)), what(what) {}
};

struct slice_access_tape {
  std::vector<slice_access> tape;

  template <typename... S>
  void record(S&&... s) {
    tape.emplace_back(slice_access{std::forward<S>(s)...});
  }
};

static std::ostream& operator<<(std::ostream& os, slice_access::type type) {
#define enum_to_string(s)     \
  case slice_access::type::s: \
    os << #s;                 \
    break;
  switch (type) {
    enum_to_string(GET) enum_to_string(HAS_KEY) enum_to_string(COPY_STRING)
        enum_to_string(IS_NUMBER) enum_to_string(IS_ARRAY) enum_to_string(IS_OBJECT)
            enum_to_string(IS_NONE) enum_to_string(LENGTH) enum_to_string(AT)
                enum_to_string(GET_NUMBER) enum_to_string(IS_STRING)
                    enum_to_string(IS_BOOL) enum_to_string(GET_BOOL)
                        enum_to_string(GET_NUMERIC_VALUE) enum_to_string(IS_EQUAL_STRING)
                            enum_to_string(STRING_VIEW) enum_to_string(ARRAY_ITER_ACCESS)
                                enum_to_string(OBJECT_ITER_ACCESS)
  }
  return os;
#undef enum_to_string
}

static inline std::ostream& operator<<(std::ostream& os, slice_access_tape const& tape) {
  for (auto const& e : tape.tape) {
    os << e.key << ' ' << e.what << ' ' << e.parameter << std::endl;
  }
  return os;
}

struct recording_slice {
  explicit recording_slice() = default;
  explicit recording_slice(arangodb::velocypack::Slice slice,
                           std::shared_ptr<slice_access_tape> tape)
    : tape(std::move(tape)), slice(slice)  {}
  explicit recording_slice(arangodb::velocypack::Slice slice,
                           std::shared_ptr<slice_access_tape> tape, std::string prefix)
    : tape(std::move(tape)), slice(slice), prefix(std::move(prefix)) {}

  std::shared_ptr<slice_access_tape> tape;
  arangodb::velocypack::Slice slice;
  std::string prefix = "$";

  bool isNumber() const;

  bool isArray() const {
    tape->record(prefix, slice_access::type::IS_ARRAY);
    return slice.isArray();
  }

  bool isString() const {
    tape->record(prefix, slice_access::type::IS_STRING);
    return slice.isString();
  }

  bool isBool() const {
    tape->record(prefix, slice_access::type::IS_BOOL);
    return slice.isBool();
  }

  bool isObject() const {
    tape->record(prefix, slice_access::type::IS_OBJECT);
    return slice.isObject();
  }

  auto length() const {
    tape->record(prefix, slice_access::type::LENGTH);
    return slice.length();
  }

  template <typename T>
  auto at(T t) const {
    tape->record(prefix + '[' + std::to_string(t) + ']', slice_access::type::AT);
    return recording_slice(slice.at(t), tape, prefix + '[' + std::to_string(t) + ']');
  }

  template <typename T>
  auto hasKey(T&& t) const {
    tape->record(prefix, slice_access::type::HAS_KEY);
    return slice.hasKey(std::forward<T>(t));
  }

  template <typename T>
  auto getNumber() const {
    tape->record(prefix, slice_access::type::GET_NUMBER);
    return slice.getNumber<T>();
  }

  auto copyString() const {
    tape->record(prefix, slice_access::type::COPY_STRING);
    return slice.copyString();
  }

  auto getBool() const {
    tape->record(prefix, slice_access::type::GET_BOOL);
    return slice.getBool();
  }

  template <typename T>
  auto isNumber() const {
    tape->record(prefix, slice_access::type::IS_NUMBER);
    return slice.isNumber<T>();
  }

  template <typename... Ts>
  auto isEqualString(Ts&&... ts) const {
    tape->record(prefix, slice_access::type::IS_EQUAL_STRING);
    return slice.isEqualString(std::forward<Ts>(ts)...);
  }

  constexpr static auto nullSlice = arangodb::velocypack::Slice::nullSlice;

  template <typename T>
  auto get(T&& t) const {
    tape->record(prefix, slice_access::type::GET, t);
    return recording_slice(slice.get(std::forward<T>(t)), tape, prefix + '.' + t);
  }

  auto toJson() const { return slice.toJson(); }

  auto isNone() const {
    tape->record(prefix, slice_access::type::IS_NONE);
    return slice.isNone();
  }

  auto stringView() const {
    tape->record(prefix, slice_access::type::STRING_VIEW);
    return slice.stringView();
  }

  static recording_slice from_buffer(arangodb::velocypack::Buffer<uint8_t> const& b) {
    return recording_slice(arangodb::velocypack::Slice(b.data()),
                           std::make_shared<slice_access_tape>());
  }
};

struct object_iterator {
  object_iterator(arangodb::velocypack::ObjectIterator const& o,
                  std::shared_ptr<slice_access_tape> tape, std::string prefix)
      : iter(o), tape(std::move(tape)), prefix(std::move(prefix)) {}
  object_iterator(recording_slice& slice, bool useSequentialIteration = false)
      : iter(slice.slice, useSequentialIteration),
        tape(slice.tape),
        prefix(slice.prefix){};

  struct pair {
    recording_slice key, value;
  };

  object_iterator begin() const { return {iter.begin(), tape, prefix}; }
  object_iterator end() const { return {iter.end(), tape, prefix}; }
  object_iterator& operator++() {
    iter.operator++();
    return *this;
  }

  bool operator!=(object_iterator const& other) const {
    return iter.operator!=(other.iter);
  }

  pair operator*() const {
    auto internal = iter.operator*();

    tape->record(prefix, slice_access::type::OBJECT_ITER_ACCESS, internal.key.copyString());
    return pair{recording_slice(internal.key, tape,
                                prefix + "@key[" + internal.key.copyString() + ']'),
                recording_slice(internal.value, tape,
                                prefix + '.' + internal.key.copyString())};
  }

  arangodb::velocypack::ObjectIterator iter;
  std::shared_ptr<slice_access_tape> tape;
  std::string prefix;
};

struct array_iterator {
  array_iterator(arangodb::velocypack::ArrayIterator const& o,
                 std::shared_ptr<slice_access_tape> tape, std::string prefix)
      : iter(o), tape(std::move(tape)), prefix(std::move(prefix)), index(0) {}
  explicit array_iterator(recording_slice& slice)
      : iter(slice.slice), tape(slice.tape), prefix(slice.prefix), index(0){};

  array_iterator begin() const { return {iter.begin(), tape, prefix}; }
  array_iterator end() const { return {iter.end(), tape, prefix}; }

  recording_slice operator*() const {
    tape->record(prefix, slice_access::type::ARRAY_ITER_ACCESS, std::to_string(index));
    auto internal = iter.operator*();
    return recording_slice(internal, tape, prefix + "[" + std::to_string(index) + ']');
  }

  bool operator!=(array_iterator const& other) const {
    return iter.operator!=(other.iter);
  }

  array_iterator& operator++() {
    ++index;
    iter.operator++();
    return *this;
  }

  array_iterator operator++(int) {
    array_iterator result(*this);
    ++(*this);
    return result;
  }

  arangodb::velocypack::ArrayIterator iter;
  std::shared_ptr<slice_access_tape> tape;
  std::string prefix;
  std::size_t index;
};

}  // namespace deserializer::test

#ifdef DESERIALIZER_SET_TEST_TYPES
namespace deserializer {
using slice_type = ::deserializer::test::recording_slice;
using object_iterator = ::deserializer::test::object_iterator;
using array_iterator = ::deserializer::test::array_iterator;
}  // namespace deserializer
#endif

#endif  // DESERIALIZER_TEST_TYPES_H

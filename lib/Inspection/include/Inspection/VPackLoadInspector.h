////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include <velocypack/ValueType.h>

#include "Inspection/LoadInspectorBase.h"
#include "Inspection/Status.h"
#include "Inspection/detail/traits.h"

namespace arangodb::inspection {

template<bool AllowUnsafeTypes, class Context = NoContext>
struct VPackLoadInspectorImpl
    : LoadInspectorBase<VPackLoadInspectorImpl<AllowUnsafeTypes, Context>,
                        velocypack::Slice, Context> {
 public:
  using Base =
      LoadInspectorBase<VPackLoadInspectorImpl, velocypack::Slice, Context>;

 public:
  explicit VPackLoadInspectorImpl(
      velocypack::Builder& builder,
      ParseOptions options = {}) requires(!Base::hasContext)
      : VPackLoadInspectorImpl(builder.slice(), options) {}

  explicit VPackLoadInspectorImpl(velocypack::Builder& builder,
                                  ParseOptions options, Context const& context)
      : VPackLoadInspectorImpl(builder.slice(), options, context) {}

  explicit VPackLoadInspectorImpl(
      velocypack::Slice slice, ParseOptions options) requires(!Base::hasContext)
      : Base(options), _slice(slice) {}

  explicit VPackLoadInspectorImpl(velocypack::Slice slice, ParseOptions options,
                                  Context const& context)
      : Base(options, context), _slice(slice) {}

  template<class T, class = std::enable_if_t<std::is_integral_v<T>>>
  [[nodiscard]] Status value(T& v) {
    try {
      v = _slice.getNumber<T>();
      return {};
    } catch (velocypack::Exception& e) {
      return {e.what()};
    }
  }

  [[nodiscard]] Status value(double& v) {
    try {
      v = _slice.getNumber<double>();
      return {};
    } catch (velocypack::Exception& e) {
      return {e.what()};
    }
  }

  [[nodiscard]] Status value(std::string& v) {
    if (!_slice.isString()) {
      return {"Expecting type String"};
    }
    v = _slice.copyString();
    return {};
  }

  [[nodiscard]] Status value(std::string_view& v) {
    static_assert(AllowUnsafeTypes);
    if (!_slice.isString()) {
      return {"Expecting type String"};
    }
    v = _slice.stringView();
    return {};
  }

  [[nodiscard]] Status value(velocypack::HashedStringRef& v) {
    static_assert(AllowUnsafeTypes);
    if (!_slice.isString()) {
      return {"Expecting type String"};
    }
    auto s = _slice.stringView();
    if (s.size() > std::numeric_limits<uint32_t>::max()) {
      return {"String value too long to store In HashedStringRef"};
    }
    v = velocypack::HashedStringRef(s.data(), static_cast<uint32_t>(s.size()));
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::Slice& v) {
    static_assert(AllowUnsafeTypes);
    v = _slice;
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::SharedSlice& v) {
    v = VPackBuilder{_slice}.sharedSlice();
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::Builder& v) {
    v.add(_slice);
    return {};
  }

  [[nodiscard]] Status value(bool& v) {
    if (!_slice.isBool()) {
      return {"Expecting type Bool"};
    }
    v = _slice.isTrue();
    return {};
  }

  [[nodiscard]] Status beginObject() {
    if (!_slice.isObject()) {
      return {"Expecting type Object"};
    }
    return {};
  }

  [[nodiscard]] Status::Success endObject() { return {}; }

  [[nodiscard]] Status beginArray() {
    if (!_slice.isArray()) {
      return {"Expecting type Array"};
    }
    return {};
  }

  [[nodiscard]] Status::Success beginField(std::string_view name) { return {}; }

  [[nodiscard]] Status::Success endField() { return {}; }

  [[nodiscard]] Status::Success endArray() { return {}; }

  bool isNull() const noexcept { return _slice.isNull(); }

  velocypack::Slice slice() noexcept { return _slice; }

 private:
  template<class>
  friend struct detail::EmbeddedFields;
  template<class, class...>
  friend struct detail::EmbeddedFieldsImpl;
  template<class, class, class>
  friend struct detail::EmbeddedFieldsWithObjectInvariant;
  template<class, class>
  friend struct detail::EmbeddedFieldInspector;

  template<class, class, class>
  friend struct LoadInspectorBase;

  auto getTypeTag() const noexcept { return _slice.type(); }

  template<class Func>
  auto doProcessObject(Func&& func)
      -> decltype(func(std::string_view(), velocypack::Slice())) {
    assert(_slice.isObject());
    for (auto [k, v] : VPackObjectIterator(slice())) {
      if (auto res = func(k.stringView(), v); not res.ok()) {
        return res;
      }
    }
    return {};
  }

  template<class Func>
  Status doProcessList(Func&& func) {
    assert(_slice.isArray());
    for (auto&& s : VPackArrayIterator(slice())) {
      if (auto res = func(s); not res.ok()) {
        return res;
      }
    }
    return {};
  }

  template<class... Ts>
  auto parseVariantInformation(
      std::string_view& type, velocypack::Slice& data,
      typename Base::template UnqualifiedVariant<Ts...>& variant) -> Status {
    if (_slice.length() > 1) {
      return {"Unqualified variant data has too many fields"};
    }
    VPackObjectIterator it(_slice);
    if (!it.valid()) {
      return {"Missing unqualified variant data"};
    }
    auto [t, v] = *it;
    assert(t.isString());
    type = t.stringView();
    data = v;
    return {};
  }

  template<class... Ts>
  auto parseVariantInformation(
      std::string_view& type, velocypack::Slice& data,
      typename Base::template QualifiedVariant<Ts...>& variant) -> Status {
    if (auto res = loadTypeField(variant.typeField, type); !res.ok()) {
      return res;
    }

    data = slice()[variant.valueField];
    if (data.isNone()) {
      return {"Variant value field \"" + std::string(variant.valueField) +
              "\" is missing"};
    }
    return {};
  }

  Status loadTypeField(std::string_view fieldName, std::string_view& result) {
    auto v = slice()[fieldName];
    if (!v.isString()) {
      if (v.isNone()) {
        return {"Variant type field \"" + std::string(fieldName) +
                "\" is missing"};
      }
      return {"Variant type field \"" + std::string(fieldName) +
              "\" must be a string"};
    }
    result = v.stringView();
    return {};
  }

  template<class T>
  bool shouldTryType(velocypack::ValueType type) {
    using velocypack::ValueType;
    auto isInt = [](ValueType type) {
      return type == ValueType::Int || type == ValueType::UInt ||
             type == ValueType::SmallInt;
    };
    // we try to rule out some cases where we know that the parse attempt will
    // definitely fail, but if none of those match, we always need to simply
    // try to parse the value
    if constexpr (std::is_same_v<T, std::string> ||
                  std::is_same_v<T, std::string_view>) {
      return type == ValueType::String;
    } else if constexpr (std::is_same_v<T, bool>) {
      return type == ValueType::Bool;
    } else if constexpr (std::is_integral_v<T>) {
      return isInt(type);
    } else if constexpr (std::is_floating_point_v<T>) {
      return type == ValueType::Double || isInt(type);
    } else if constexpr (detail::IsTuple<T>::value) {
      return type == ValueType::Array &&
             detail::TupleSize<T>::value == _slice.length();
    } else if constexpr (detail::IsListLike<T>::value) {
      return type == ValueType::Array;
    } else if constexpr (detail::IsSetLike<T>::value) {
      return type == ValueType::Array;
    } else if constexpr (detail::IsMapLike<T>::value) {
      return type == ValueType::Object;
    }
    return true;
  }

  template<std::size_t Idx, std::size_t End, class T>
  [[nodiscard]] Status processTuple(T& data) {
    if constexpr (Idx < End) {
      auto ff = this->make(_slice[Idx]);
      if (auto res = process(ff, std::get<Idx>(data)); !res.ok()) {
        return {std::move(res), std::to_string(Idx), Status::ArrayTag{}};
      }
      return {processTuple<Idx + 1, End>(data)};
    } else {
      return {};
    }
  }

  Status checkArrayLength(std::size_t arrayLength) {
    assert(_slice.isArray());
    if (_slice.length() != arrayLength) {
      return {"Expected array of length " + std::to_string(arrayLength)};
    }
    return {};
  }

  velocypack::Slice _slice;
};

template<class Context = NoContext>
using VPackLoadInspector = VPackLoadInspectorImpl<false, Context>;
template<class Context = NoContext>
using VPackUnsafeLoadInspector = VPackLoadInspectorImpl<true, Context>;

}  // namespace arangodb::inspection

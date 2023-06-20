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

#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include "Inspection/LoadInspectorBase.h"
#include "Inspection/VPackLoadInspector.h"
#include "Agency/Node.h"

namespace arangodb::inspection {

template<bool AllowUnsafeTypes, class Context = NoContext>
struct NodeLoadInspectorImpl
    : LoadInspectorBase<NodeLoadInspectorImpl<AllowUnsafeTypes, Context>,
                        consensus::Node const*, Context> {
 protected:
  using Base =
      LoadInspectorBase<NodeLoadInspectorImpl, consensus::Node const*, Context>;

 public:
  explicit NodeLoadInspectorImpl(
      consensus::Node const* node,
      ParseOptions options = {}) requires(!Base::hasContext)
      : Base(options), _node(node) {}
  explicit NodeLoadInspectorImpl(
      consensus::NodePtr const& node,
      ParseOptions options = {}) requires(!Base::hasContext)
      : Base(options), _node(node.get()) {}

  explicit NodeLoadInspectorImpl(consensus::Node const* node,
                                 ParseOptions options, Context const& context)
      : Base(options, context), _node(node) {}
  explicit NodeLoadInspectorImpl(consensus::NodePtr const& node,
                                 ParseOptions options, Context const& context)
      : Base(options, context), _node(node.get()) {}

  template<class T, class = std::enable_if_t<std::is_integral_v<T>>>
  [[nodiscard]] Status value(T& v) {
    try {
      v = _node->slice().getNumber<T>();
      return {};
    } catch (velocypack::Exception& e) {
      return {e.what()};
    }
  }

  [[nodiscard]] Status value(double& v) {
    try {
      v = _node->slice().getNumber<double>();
      return {};
    } catch (velocypack::Exception& e) {
      return {e.what()};
    }
  }

  [[nodiscard]] Status value(std::string& v) {
    if (auto value = _node->getString(); value) {
      v = std::move(*value);
      return {};
    }
    return {"Expecting type String"};
  }

  [[nodiscard]] Status value(std::string_view& v) {
    static_assert(AllowUnsafeTypes);
    if (!_node->isString()) {
      return {"Expecting type String"};
    }
    v = *_node->getStringView();
    return {};
  }

  [[nodiscard]] Status value(velocypack::HashedStringRef& v) {
    static_assert(AllowUnsafeTypes);
    if (!_node->isString()) {
      return {"Expecting type String"};
    }
    auto s = *_node->getStringView();
    if (s.size() > std::numeric_limits<uint32_t>::max()) {
      return {"String value too long to store In HashedStringRef"};
    }
    v = velocypack::HashedStringRef(s.data(), static_cast<uint32_t>(s.size()));
    return {};
  }

  [[nodiscard]] Status value(velocypack::Slice& v) {
    static_assert(AllowUnsafeTypes);
    if (!_node->isPrimitiveValue()) {
      return {"Cannot parse non-primitive node as Slice."};
    }
    v = _node->slice();
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::SharedSlice& v) {
    if (_node->isPrimitiveValue()) {
      if constexpr (AllowUnsafeTypes) {
        v = velocypack::SharedSlice(velocypack::SharedSlice{}, _node->slice());
        return {};
      } else {
        auto slice = _node->slice();
        velocypack::Buffer<std::uint8_t> buffer(slice.byteSize());
        buffer.append(slice.start(), slice.byteSize());
        v = velocypack::SharedSlice(std::move(buffer));
        return {};
      }
    } else {
      velocypack::Buffer<uint8_t> buffer;
      velocypack::Builder builder(buffer);
      _node->toBuilder(builder);
      v = velocypack::SharedSlice(std::move(buffer));
      return {};
    }
  }

  [[nodiscard]] Status::Success value(velocypack::Builder& v) {
    _node->toBuilder(v);
    return {};
  }

  [[nodiscard]] Status value(bool& v) {
    if (auto value = _node->getBool(); value) {
      v = std::move(*value);
      return {};
    }
    return {"Expecting type Bool"};
  }

  [[nodiscard]] Status beginObject() {
    if (!_node->isObject()) {
      return {"Expecting type Object"};
    }
    return {};
  }

  [[nodiscard]] Status::Success endObject() { return {}; }

  [[nodiscard]] Status beginArray() {
    if (!_node->isArray()) {
      return {"Expecting type Array"};
    }
    return {};
  }

  [[nodiscard]] Status::Success endArray() { return {}; }

  bool isNull() const noexcept {
    return _node == nullptr || _node->slice().isNull();
  }

  consensus::Node const* node() const noexcept { return _node; }

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

  auto make(velocypack::Slice data) {
    if constexpr (Base::hasContext) {
      return VPackLoadInspectorImpl<AllowUnsafeTypes, Context>(
          data, this->options(), this->getContext());
    } else {
      return VPackLoadInspectorImpl<AllowUnsafeTypes>(data, this->options());
    }
  }

  auto getTypeTag() const noexcept {
    if (_node->isPrimitiveValue()) {
      return _node->slice().type();
    } else if (_node->isArray()) {
      return velocypack::ValueType::Array;
    } else {
      return velocypack::ValueType::Object;
    }
  }

  template<class Func>
  auto doProcessObject(Func&& func)
      -> decltype(func(std::string_view(),
                       std::declval<consensus::Node const*>())) {
    assert(_node);
    assert(_node->isObject());
    for (auto& [k, v] : _node->children()) {
      if (auto res = func(k, v.get()); !res.ok()) {
        return res;
      }
    }
    return {};
  }

  template<class Func>
  Status doProcessList(Func&& func) {
    assert(_node->isArray());
    for (auto&& s : *_node->getArray()) {
      if (auto res = func(s); !res.ok()) {
        return res;
      }
    }
    return {};
  }

  template<class... Ts>
  auto parseVariantInformation(
      std::string_view& type, consensus::Node const*& data,
      typename Base::template UnqualifiedVariant<Ts...>& variant) -> Status {
    auto& children = _node->children();
    if (children.empty()) {
      return {"Missing unqualified variant data"};
    }
    if (children.size() > 1) {
      return {"Unqualified variant data has too many fields"};
    }
    auto& [t, v] = *children.begin();
    type = t;
    data = v.get();
    return {};
  }

  template<class... Ts>
  auto parseVariantInformation(
      std::string_view& type, consensus::Node const*& data,
      typename Base::template QualifiedVariant<Ts...>& variant) -> Status {
    if (auto res = loadTypeField(variant.typeField, type); !res.ok()) {
      return res;
    }

    auto node = _node->get(variant.valueField);
    if (!node) {
      return {"Variant value field \"" + std::string(variant.valueField) +
              "\" is missing"};
    }
    data = node.get();
    return {};
  }

  Status loadTypeField(std::string_view fieldName, std::string_view& result) {
    auto v = _node->get(fieldName);
    if (!v) {
      return {"Variant type field \"" + std::string(fieldName) +
              "\" is missing"};
    }
    auto val = v->getStringView();
    if (!val.has_value()) {
      return {"Variant type field \"" + std::string(fieldName) +
              "\" must be a string"};
    }
    result = *val;
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
      return type == ValueType::Array && _node->isArray() &&
             detail::TupleSize<T>::value == _node->getArray()->size();
    } else if constexpr (detail::IsListLike<T>::value) {
      return type == ValueType::Array;
    } else if constexpr (detail::IsMapLike<T>::value) {
      return type == ValueType::Object;
    }
    return true;
  }

  template<std::size_t Idx, std::size_t End, class T>
  [[nodiscard]] Status processTuple(T& data) {
    auto slice = _node->getArray();
    assert(slice.has_value());
    if constexpr (Idx < End) {
      auto ff = make(slice->operator[](Idx));
      if (auto res = process(ff, std::get<Idx>(data)); !res.ok()) {
        return {std::move(res), std::to_string(Idx), Status::ArrayTag{}};
      }
      return {processTuple<Idx + 1, End>(data)};
    } else {
      return {};
    }
  }

  Status checkArrayLength(std::size_t arrayLength) {
    auto slice = _node->getArray();
    assert(slice != nullptr);
    if (slice->size() != arrayLength) {
      return {"Expected array of length " + std::to_string(arrayLength)};
    }
    return {};
  }

  consensus::Node const* _node;
};

template<class Context = NoContext>
using NodeLoadInspector = NodeLoadInspectorImpl<false, Context>;
template<class Context = NoContext>
using NodeUnsafeLoadInspector = NodeLoadInspectorImpl<true, Context>;

}  // namespace arangodb::inspection

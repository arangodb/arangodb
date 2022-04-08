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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include "Inspection/InspectorBase.h"
#include "Inspection/VPackLoadInspector.h"
#include "Agency/Node.h"

namespace arangodb::inspection {

struct NodeLoadInspector : InspectorBase<NodeLoadInspector> {
  static constexpr bool isLoading = true;

  explicit NodeLoadInspector(consensus::Node const& node, ParseOptions options)
      : _node(node), _options(options) {}

  template<class T, class = std::enable_if_t<std::is_integral_v<T>>>
  [[nodiscard]] Status value(T& v) {
    if (auto value = _node.getNumber<T>(); value) {
      v = *value;
    } else {
      return {"Expecting numeric type"};
    }
  }

  [[nodiscard]] Status value(double& v) {
    if (auto value = _node.getDouble(); value) {
      v = std::move(*value);
      return {};
    } else {
      return {"Expecting type Double"};
    }
  }

  [[nodiscard]] Status value(std::string& v) {
    if (auto value = _node.getString(); value) {
      v = std::move(*value);
      return {};
    } else {
      return {"Expecting type String"};
    }
  }

  [[nodiscard]] Status value(bool& v) {
    if (auto value = _node.getBool(); value) {
      v = std::move(*value);
      return {};
    } else {
      return {"Expecting type Bool"};
    }
  }

  [[nodiscard]] Status beginObject() {
    if (!_node.isObject()) {
      return {"Expecting type Object"};
    }
    return {};
  }

  [[nodiscard]] Status::Success endObject() { return {}; }

  [[nodiscard]] Status beginArray() {
    if (!_node.isArray()) {
      return {"Expecting type Array"};
    }
    return {};
  }

  [[nodiscard]] Status::Success endArray() { return {}; }

  template<class T>
  [[nodiscard]] Status list(T& list) {
    return beginArray()                           //
           | [&]() { return processList(list); }  //
           | [&]() { return endArray(); };        //
  }

  template<class T>
  [[nodiscard]] Status map(T& map) {
    return beginObject()                        //
           | [&]() { return processMap(map); }  //
           | [&]() { return endObject(); };     //
  }

  template<class T>
  [[nodiscard]] Status tuple(T& data) {
    constexpr auto arrayLength = std::tuple_size_v<T>;

    return beginArray()                                            //
           | [&]() { return checkArrayLength(arrayLength); }       //
           | [&]() { return processTuple<0, arrayLength>(data); }  //
           | [&]() { return endArray(); };                         //
  }

  template<class T, size_t N>
  [[nodiscard]] Status tuple(T (&data)[N]) {
    return beginArray()                             //
           | [&]() { return checkArrayLength(N); }  //
           | [&]() { return processArray(data); }   //
           | [&]() { return endArray(); };          //
  }

  [[nodiscard]] Status::Success parseField(velocypack::Slice, IgnoreField&&) {
    return {};
  }

  template<class T>
  [[nodiscard]] Status parseField(std::shared_ptr<consensus::Node> node, T&& field) {

    auto name = getFieldName(field);
    auto& value = getFieldValue(field);
    auto load = [&]() {
      auto isPresent = node != nullptr;
      NodeLoadInspector ff(*node, _options);
      using FallbackField = decltype(getFallbackField(field));
      if constexpr (!std::is_void_v<FallbackField>) {
        auto applyFallback = [&](auto& val) {
          getFallbackField(field).apply(val);
        };
        if constexpr (!std::is_void_v<decltype(getTransformer(field))>) {
          return loadTransformedField(ff, name, isPresent, value,
                                      std::move(applyFallback),
                                      getTransformer(field));
        } else {
          return loadField(ff, name, isPresent, value,
                           std::move(applyFallback));
        }
      } else {
        if constexpr (!std::is_void_v<decltype(getTransformer(field))>) {
          return loadTransformedField(ff, name, isPresent, value,
                                      getTransformer(field));
        } else {
          return loadField(ff, name, isPresent, value);
        }
      }
    };

    auto res = load()                                      //
               | [&]() { return checkInvariant(field); };  //

    if (!res.ok()) {
      return {std::move(res), name, Status::AttributeTag{}};
    }
    return res;
  }

  ParseOptions options() const noexcept { return _options; }

  template<class U>
  struct FallbackContainer {
    explicit FallbackContainer(U&& val) : fallbackValue(std::move(val)) {}
    template<class T>
    void apply(T& val) const noexcept {
      if constexpr (std::is_assignable_v<T, U>) {
        val = fallbackValue;
      } else {
        val = T{fallbackValue};
      }
    }

   private:
    U fallbackValue;
  };

  template<class Invariant>
  struct InvariantContainer {
    explicit InvariantContainer(Invariant&& invariant)
        : invariantFunc(std::move(invariant)) {}
    Invariant invariantFunc;

    static constexpr const char InvariantFailedError[] =
        "Field invariant failed";
  };

  template<class... Args>
  Status applyFields(Args&&... args) {
    std::array<std::string_view, sizeof...(args)> names{getFieldName(args)...};
    std::array<std::shared_ptr<consensus::Node>, sizeof...(args)> slices;
    for (auto [k, v] : _node.children()) {
      auto it = std::find(names.begin(), names.end(), k);
      if (it != names.end()) {
        slices[std::distance(names.begin(), it)] = v;
      } else if (_options.ignoreUnknownFields) {
        continue;
      } else {
        return {"Found unexpected attribute '" + k + "'"};
      }
    }
    return parseFields(slices.data(), std::forward<Args>(args)...);
  }

 private:
  template<class T>
  struct HasFallback : std::false_type {};

  template<class T, class U>
  struct HasFallback<FallbackField<T, U>> : std::true_type {};

  template<class Arg>
  Status parseFields(std::shared_ptr<consensus::Node>* slices, Arg&& arg) {
    return parseField(*slices, std::forward<Arg>(arg));
  }

  template<class Arg, class... Args>
  Status parseFields(std::shared_ptr<consensus::Node>* slices, Arg&& arg, Args&&... args) {
    return parseField(*slices, std::forward<Arg>(arg)) |
           [&]() { return parseFields(++slices, std::forward<Args>(args)...); };
  }

  template<class T>
  Status processList(T& list) {
    auto slice = _node.getArray();
    if (!slice) {
      return {"Expected array"};
    }
    std::size_t idx = 0;
    for (auto&& s : VPackArrayIterator(*slice)) {
      VPackLoadInspector ff(s, _options);
      typename T::value_type val;
      if (auto res = process(ff, val); !res.ok()) {
        return {std::move(res), std::to_string(idx), Status::ArrayTag{}};
      }
      list.push_back(std::move(val));
      ++idx;
    }
    return {};
  }

  template<class T>
  Status processMap(T& map) {
    for (auto&& pair : _node.children()) {
      NodeLoadInspector ff(*pair.second, _options);
      typename T::mapped_type val;
      if (auto res = process(ff, val); !res.ok()) {
        return {std::move(res), "'" + pair.first + "'",
                Status::ArrayTag{}};
      }
      map.emplace(pair.first, std::move(val));
    }
    return {};
  }

  template<class T, class U>
  Status checkInvariant(InvariantField<T, U>& field) {
    return InspectorBase::checkInvariant<
        InvariantField<T, U>, InvariantField<T, U>::InvariantFailedError>(
        field.invariantFunc, getFieldValue(field));
  }

  template<class T>
  auto checkInvariant(T& field) {
    if constexpr (!IsRawField<std::remove_cvref_t<T>>::value) {
      return checkInvariant(field.inner);
    } else {
      return Status::Success{};
    }
  }

  template<std::size_t Idx, std::size_t End, class T>
  [[nodiscard]] Status processTuple(T& data) {
    auto slice = _node.getArray();
    if (!slice) {
      return {"Expected array"};
    }
    if constexpr (Idx < End) {
      VPackLoadInspector ff{slice->operator[](Idx), _options};
      if (auto res = process(ff, std::get<Idx>(data)); !res.ok()) {
        return {std::move(res), std::to_string(Idx), Status::ArrayTag{}};
      }
      return {processTuple<Idx + 1, End>(data)};
    } else {
      return {};
    }
  }

  template<class T, size_t N>
  [[nodiscard]] Status processArray(T (&data)[N]) {
    std::size_t index = 0;
    auto slice = _node.getArray();
    if (!slice) {
      return {"Expected array"};
    }
    for (auto&& v : VPackArrayIterator(*slice)) {
      VPackLoadInspector ff(v, _options);
      if (auto res = process(ff, data[index]); !res.ok()) {
        return {std::move(res), std::to_string(index), Status::ArrayTag{}};
      }
      ++index;
    }
    return {};
  }

  Status checkArrayLength(std::size_t arrayLength) {
    auto slice = _node.getArray();
    if (!slice) {
      return {"Expected array"};
    }
    if (slice->length() != arrayLength) {
      return {"Expected array of length " + std::to_string(arrayLength)};
    }
    return {};
  }

  consensus::Node const& _node;
  ParseOptions _options;
};

template<>
struct NodeLoadInspector::FallbackContainer<NodeLoadInspector::Keep> {
  explicit FallbackContainer(NodeLoadInspector::Keep&&) {}
  template<class T>
  void apply(T&) const noexcept {}
};

}  // namespace arangodb::inspection

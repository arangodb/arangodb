
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
#include <variant>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include "Basics/overload.h"
#include "Inspection/InspectorBase.h"

namespace arangodb::inspection {

struct VPackSaveInspector : InspectorBase<VPackSaveInspector> {
  static constexpr bool isLoading = false;

  explicit VPackSaveInspector(velocypack::Builder& builder)
      : _builder(builder) {}

  template<class T>
  [[nodiscard]] Status apply(T const& x) {
    return process(*this, x);
  }

  [[nodiscard]] Status::Success beginObject() {
    _builder.openObject();
    return {};
  }

  [[nodiscard]] Status::Success endObject() {
    _builder.close();
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::Slice s) {
    _builder.add(s);
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::HashedStringRef const& s) {
    _builder.add(VPackValue(s.stringView()));
    return {};
  }

  template<class T, class = std::enable_if_t<detail::IsBuiltinType<T>()>>
  [[nodiscard]] Status::Success value(T const& v) {
    static_assert(detail::IsBuiltinType<T>());
    _builder.add(VPackValue(v));
    return {};
  }

  [[nodiscard]] Status::Success beginArray() {
    _builder.openArray();
    return {};
  }

  [[nodiscard]] Status::Success endArray() {
    _builder.close();
    return {};
  }

  template<class T>
  [[nodiscard]] auto tuple(T const& data) {
    return beginArray()                                                     //
           | [&]() { return processTuple<0, std::tuple_size_v<T>>(data); }  //
           | [&]() { return endArray(); };                                  //
  }

  template<class T, size_t N>
  [[nodiscard]] auto tuple(T (&data)[N]) {
    return beginArray()                                                       //
           | [&]() { return processList(std::begin(data), std::end(data)); }  //
           | [&]() { return endArray(); };                                    //
  }

  template<class T>
  [[nodiscard]] auto list(T const& list) {
    return beginArray()                                                       //
           | [&]() { return processList(std::begin(list), std::end(list)); }  //
           | [&]() { return endArray(); };                                    //
  }

  template<class T>
  [[nodiscard]] auto map(T const& map) {
    return beginObject()                        //
           | [&]() { return processMap(map); }  //
           | [&]() { return endObject(); };     //
  }

  template<class Arg, class... Args>
  auto applyFields(Arg&& arg, Args&&... args) {
    auto res = self().applyField(std::forward<Arg>(arg));
    if constexpr (sizeof...(args) == 0) {
      return res;
    } else {
      return std::move(res)                                                 //
             | [&]() { return applyFields(std::forward<Args>(args)...); };  //
    }
  }

  template<class... Ts, class... Args>
  auto processVariant(UnqualifiedVariant<Ts...>& variant, Args&&... args) {
    return beginObject()  //
           |
           [&]() {
             return std::visit(overload{[this, &args](typename Args::Type& v) {
                                 return applyFields(field(args.tag, v));
                               }...},
                               variant.value);
           }  //
           | [&]() { return endObject(); };
  }

  template<class... Ts, class... Args>
  auto processVariant(QualifiedVariant<Ts...>& variant, Args&&... args) {
    return beginObject()  //
           |
           [&]() {
             return std::visit(
                 overload{[this, &variant, &args](typename Args::Type& v) {
                   return applyFields(field(variant.typeField, args.tag),
                                      field(variant.valueField, v));
                 }...},
                 variant.value);
           }  //
           | [&]() { return endObject(); };
  }

  velocypack::Builder& builder() noexcept { return _builder; }

  template<class U>
  struct FallbackContainer {
    explicit FallbackContainer(U&&) {}
  };
  template<class T>
  struct InvariantContainer {
    explicit InvariantContainer(T&&) {}
  };

 private:
  template<class T>
  [[nodiscard]] auto applyField(T const& field) {
    if constexpr (std::is_same_v<IgnoreField, T>) {
      return Status::Success{};
    } else {
      auto name = getFieldName(field);
      auto& value = getFieldValue(field);
      constexpr bool hasFallback =
          !std::is_void_v<decltype(getFallbackField(field))>;
      auto res = [&]() {
        if constexpr (!std::is_void_v<decltype(getTransformer(field))>) {
          return saveTransformedField(*this, name, hasFallback, value,
                                      getTransformer(field));
        } else {
          return saveField(*this, name, hasFallback, value);
        }
      }();
      if constexpr (!isSuccess(res)) {
        if (!res.ok()) {
          return Status{std::move(res), name, Status::AttributeTag{}};
        }
      }
      return res;
    }
  }

  template<std::size_t Idx, std::size_t End, class T>
  [[nodiscard]] auto processTuple(T const& data) {
    if constexpr (Idx < End) {
      return process(*this, std::get<Idx>(data))                    //
             | [&]() { return processTuple<Idx + 1, End>(data); };  //
    } else {
      return Status::Success{};
    }
  }

  template<class Iterator>
  [[nodiscard]] auto processList(Iterator begin, Iterator end)
      -> decltype(process(std::declval<VPackSaveInspector&>(), *begin)) {
    for (auto it = begin; it != end; ++it) {
      if (auto res = process(*this, *it); !res.ok()) {
        return res;
      }
    }
    return {};
  }
  template<class T>
  [[nodiscard]] auto processMap(T const& map)
      -> decltype(process(std::declval<VPackSaveInspector&>(),
                          map.begin()->second)) {
    for (auto&& [k, v] : map) {
      _builder.add(VPackValue(k));
      if (auto res = process(*this, v); !res.ok()) {
        return res;
      }
    }
    return {};
  }

  velocypack::Builder& _builder;
};

}  // namespace arangodb::inspection

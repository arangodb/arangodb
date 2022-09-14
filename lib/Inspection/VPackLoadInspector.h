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

#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include "Inspection/InspectorBase.h"
#include "Inspection/Status.h"

namespace arangodb::inspection {

struct ParseOptions {
  bool ignoreUnknownFields = false;
  bool ignoreMissingFields = false;
};

template<bool AllowUnsafeTypes, class Context = NoContext>
struct VPackLoadInspectorImpl
    : InspectorBase<VPackLoadInspectorImpl<AllowUnsafeTypes, Context>,
                    Context> {
 protected:
  using Base = InspectorBase<VPackLoadInspectorImpl, Context>;

 public:
  static constexpr bool isLoading = true;

  explicit VPackLoadInspectorImpl(
      velocypack::Builder& builder,
      ParseOptions options = {}) requires(!Base::hasContext)
      : VPackLoadInspectorImpl(builder.slice(), options) {}

  explicit VPackLoadInspectorImpl(velocypack::Builder& builder,
                                  ParseOptions options, Context const& context)
      : VPackLoadInspectorImpl(builder.slice(), options, context) {}

  explicit VPackLoadInspectorImpl(
      velocypack::Slice slice, ParseOptions options) requires(!Base::hasContext)
      : _slice(slice), _options(options) {}

  explicit VPackLoadInspectorImpl(velocypack::Slice slice, ParseOptions options,
                                  Context const& context)
      : Base(context), _slice(slice), _options(options) {}

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

  [[nodiscard]] Status::Success parseField(velocypack::Slice,
                                           typename Base::IgnoreField&&) {
    return {};
  }

  template<class T>
  [[nodiscard]] Status parseField(velocypack::Slice slice, T&& field) {
    auto ff = make(slice);
    auto name = Base::getFieldName(field);
    auto& value = Base::getFieldValue(field);
    auto load = [&]() -> Status {
      auto isPresent = !slice.isNone();
      using FallbackField = decltype(Base::getFallbackField(field));
      if constexpr (!std::is_void_v<FallbackField>) {
        auto applyFallback = [&](auto& val) {
          Base::getFallbackField(field).apply(val);
        };
        if constexpr (!std::is_void_v<decltype(Base::getTransformer(field))>) {
          return loadTransformedField(ff, name, isPresent, value,
                                      std::move(applyFallback),
                                      Base::getTransformer(field));
        } else {
          return loadField(ff, name, isPresent, value,
                           std::move(applyFallback));
        }
      } else {
        if (!isPresent && _options.ignoreMissingFields) {
          return {};
        }
        if constexpr (!std::is_void_v<decltype(Base::getTransformer(field))>) {
          return loadTransformedField(ff, name, isPresent, value,
                                      Base::getTransformer(field));
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

  velocypack::Slice slice() noexcept { return _slice; }

  ParseOptions options() const noexcept { return _options; }

  template<class U>
  struct ActualFallbackContainer {
    explicit ActualFallbackContainer(U&& val) : fallbackValue(std::move(val)) {}
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

  struct EmptyFallbackContainer {
    explicit EmptyFallbackContainer(typename Base::Keep&&) {}
    template<class T>
    void apply(T&) const noexcept {}
  };

  template<class U>
  using FallbackContainer =
      std::conditional_t<std::is_same_v<U, typename Base::Keep>,
                         EmptyFallbackContainer, ActualFallbackContainer<U>>;

  template<class Fn>
  struct FallbackFactoryContainer {
    explicit FallbackFactoryContainer(Fn&& fn) : factory(std::move(fn)) {}
    template<class T>
    void apply(T& val) const noexcept {
      if constexpr (std::is_assignable_v<T, std::invoke_result_t<Fn>>) {
        val = std::invoke(factory);
      } else {
        val = T{std::invoke(factory)};
      }
    }

   private:
    Fn factory;
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
    std::array<std::string_view, sizeof...(args)> names{
        Base::getFieldName(args)...};
    std::array<velocypack::Slice, sizeof...(args)> slices;
    for (auto [k, v] : VPackObjectIterator(slice())) {
      auto it = std::find(names.begin(), names.end(), k.stringView());
      if (it != names.end()) {
        slices[std::distance(names.begin(), it)] = v;
      } else if (_options.ignoreUnknownFields) {
        continue;
      } else {
        return {"Found unexpected attribute '" + k.copyString() + "'"};
      }
    }
    return parseFields(slices.data(), std::forward<Args>(args)...);
  }

  template<class... Ts, class... Args>
  auto processVariant(
      typename Base::template UnqualifiedVariant<Ts...>& variant,
      Args&&... args) {
    auto loadVariant = [&]() -> Status {
      VPackObjectIterator it(_slice);
      if (!it.valid()) {
        return {"Missing unqualified variant data"};
      }
      auto [type, value] = *it;
      TRI_ASSERT(type.isString());
      return parseType(type.stringView(), value, variant.value,
                       std::forward<Args>(args)...);
    };
    return beginObject()                     //
           | loadVariant                     //
           | [&]() { return endObject(); };  //
  }

  template<class... Ts, class... Args>
  auto processVariant(typename Base::template QualifiedVariant<Ts...>& variant,
                      Args&&... args) {
    auto loadVariant = [&]() -> Status {
      auto type = slice()[variant.typeField];
      if (!type.isString()) {
        if (type.isNone()) {
          return {"Variant type field \"" + std::string(variant.typeField) +
                  "\" is missing"};
        } else {
          return {"Variant type field \"" + std::string(variant.typeField) +
                  "\" must be a string"};
        }
      }

      auto value = slice()[variant.valueField];
      if (value.isNone()) {
        return {"Variant value field \"" + std::string(variant.valueField) +
                "\" is missing"};
      }

      return parseType(type.stringView(), value, variant.value,
                       std::forward<Args>(args)...);
    };
    return beginObject()                     //
           | loadVariant                     //
           | [&]() { return endObject(); };  //
  }

 protected:
  auto make(velocypack::Slice slice) {
    if constexpr (Base::hasContext) {
      return VPackLoadInspectorImpl(slice, _options, this->getContext());
    } else {
      return VPackLoadInspectorImpl(slice, _options);
    }
  }
  Status::Success parseFields(velocypack::Slice* slices) {
    return Status::Success{};
  }

  template<class Arg>
  Status parseFields(velocypack::Slice* slices, Arg&& arg) {
    return parseField(*slices, std::forward<Arg>(arg));
  }

  template<class Arg, class... Args>
  Status parseFields(velocypack::Slice* slices, Arg&& arg, Args&&... args) {
    return parseField(*slices, std::forward<Arg>(arg)) |
           [&]() { return parseFields(++slices, std::forward<Args>(args)...); };
  }

  template<class... Ts, class Arg, class... Args>
  Status parseType(std::string_view tag, velocypack::Slice value,
                   std::variant<Ts...>& result, Arg&& arg, Args&&... args) {
    if (arg.tag == tag) {
      auto inspector = make(value);
      typename Arg::Type v;
      auto res = inspector.apply(v);
      if (res.ok()) {
        result = v;
        return res;
      } else {
        return {std::move(res), "value", Status::AttributeTag{}};
      }
    } else {
      if constexpr (sizeof...(Args) == 0) {
        return {"Found invalid type: " + std::string(tag)};
      } else {
        return parseType(tag, value, result, std::forward<Args>(args)...);
      }
    }
  }

  template<class T>
  Status processList(T& list) {
    std::size_t idx = 0;
    for (auto&& s : VPackArrayIterator(_slice)) {
      auto ff = make(s);
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
    for (auto&& pair : VPackObjectIterator(_slice)) {
      auto ff = make(pair.value);
      typename T::mapped_type val;
      if (auto res = process(ff, val); !res.ok()) {
        return {std::move(res), "'" + pair.key.copyString() + "'",
                Status::ArrayTag{}};
      }
      map.emplace(pair.key.copyString(), std::move(val));
    }
    return {};
  }

  template<class T, class U>
  Status checkInvariant(typename Base::template InvariantField<T, U>& field) {
    using Field = typename Base::template InvariantField<T, U>;
    return Base::template checkInvariant<Field, Field::InvariantFailedError>(
        field.invariantFunc, Base::getFieldValue(field));
  }

  template<class T>
  auto checkInvariant(T& field) {
    if constexpr (!Base::template IsRawField<std::remove_cvref_t<T>>::value) {
      return checkInvariant(field.inner);
    } else {
      return Status::Success{};
    }
  }

  template<std::size_t Idx, std::size_t End, class T>
  [[nodiscard]] Status processTuple(T& data) {
    if constexpr (Idx < End) {
      auto ff = make(_slice[Idx]);
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
    for (auto&& v : VPackArrayIterator(_slice)) {
      auto ff = make(v);
      if (auto res = process(ff, data[index]); !res.ok()) {
        return {std::move(res), std::to_string(index), Status::ArrayTag{}};
      }
      ++index;
    }
    return {};
  }

  Status checkArrayLength(std::size_t arrayLength) {
    if (_slice.length() != arrayLength) {
      return {"Expected array of length " + std::to_string(arrayLength)};
    }
    return {};
  }

  velocypack::Slice _slice;
  ParseOptions _options;
};

template<class Context = NoContext>
using VPackLoadInspector = VPackLoadInspectorImpl<false, Context>;
template<class Context = NoContext>
using VPackUnsafeLoadInspector = VPackLoadInspectorImpl<true, Context>;

}  // namespace arangodb::inspection

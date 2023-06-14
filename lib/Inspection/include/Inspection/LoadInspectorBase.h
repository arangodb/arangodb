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

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

#include "Inspection/InspectorBase.h"
#include "Inspection/Factory.h"

namespace arangodb::inspection {

struct ParseOptions {
  bool ignoreUnknownFields = false;
  bool ignoreMissingFields = false;
};

template<class Derived, class ValueType, class Context>
struct LoadInspectorBase : InspectorBase<Derived, Context> {
 protected:
  using Base = InspectorBase<Derived, Context>;

 public:
  static constexpr bool isLoading = true;

  explicit LoadInspectorBase(ParseOptions options) requires(!Base::hasContext)
      : _options(options) {}

  explicit LoadInspectorBase(ParseOptions options, Context const& context)
      : Base(context), _options(options) {}

  template<class T>
  [[nodiscard]] Status list(T& list) {
    auto& f = this->self();
    return f.beginArray()                           //
           | [&]() { return f.processList(list); }  //
           | [&]() { return f.endArray(); };        //
  }

  template<class T>
  [[nodiscard]] Status map(T& map) {
    auto& f = this->self();
    return f.beginObject()                        //
           | [&]() { return f.processMap(map); }  //
           | [&]() { return f.endObject(); };     //
  }

  template<class T>
  [[nodiscard]] Status tuple(T& data) {
    constexpr auto arrayLength = std::tuple_size_v<T>;
    auto& f = this->self();
    return f.beginArray()                                                     //
           | [&]() { return f.checkArrayLength(arrayLength); }                //
           | [&]() { return f.template processTuple<0, arrayLength>(data); }  //
           | [&]() { return f.endArray(); };                                  //
  }

  template<class T, size_t N>
  [[nodiscard]] Status tuple(T (&data)[N]) {
    auto& f = this->self();
    return f.beginArray()                             //
           | [&]() { return f.checkArrayLength(N); }  //
           | [&]() { return f.processArray(data); }   //
           | [&]() { return f.endArray(); };          //
  }

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
  };

  template<class Invariant, class T>
  Status objectInvariant(T& object, Invariant&& func, Status result) {
    if (result.ok()) {
      result =
          Base::template doCheckInvariant<detail::ObjectInvariantFailedError>(
              std::forward<Invariant>(func), object);
    }
    return result;
  }

  template<class... Args>
  Status applyFields(Args&&... args) {
    FieldsMap fields;
    this->self().doProcessObject([&](std::string_view key, ValueType value) {
      fields.emplace(key, std::make_pair(std::move(value), false));
      return Status::Success{};
    });

    auto result = parseFields(fields, std::forward<Args>(args)...);
    if (result.ok() && !_options.ignoreUnknownFields) {
      for (auto& [k, v] : fields) {
        if (!v.second) {
          return {"Found unexpected attribute '" + std::string(k) + "'"};
        }
      }
    }
    return result;
  }

  template<class T, class Arg, class... Args>
  auto processVariant(T& variant, Arg&& arg, Args&&... args) {
    if constexpr (Arg::isInlineType) {
      auto type = this->self().getTypeTag();
      return this->parseVariant(type, variant, std::forward<Arg>(arg),
                                std::forward<Args>(args)...);
    } else {
      return this->parseNonInlineTypes(variant, std::forward<Arg>(arg),
                                       std::forward<Args>(args)...);
    }
  }

 protected:
  using FieldsMap =
      std::unordered_map<std::string_view, std::pair<ValueType, bool>>;

  using EmbeddedParam = FieldsMap;

  auto make(ValueType data) {
    if constexpr (Base::hasContext) {
      return Derived(data, _options, this->getContext());
    } else {
      return Derived(data, _options);
    }
  }

  template<class... Args>
  Status processEmbeddedFields(EmbeddedParam& fields, Args&&... args) {
    return parseFields(fields, std::forward<Args>(args)...);
  }

  template<class TypeTag, class Variant, class Arg, class... Args>
  Status parseVariant(TypeTag type, Variant& variant, Arg&& arg,
                      Args&&... args) {
    if constexpr (Arg::isInlineType) {
      if (this->self().template shouldTryType<typename Arg::Type>(type)) {
        auto v = Factory<typename Arg::Type>::create();
        auto res = this->apply(v);
        if (res.ok()) {
          variant.value = std::move(v);
          return res;
        }
      }

      if constexpr (sizeof...(Args) == 0) {
        return {"Could not find matching inline type"};
      } else {
        return parseVariant(type, variant, std::forward<Args>(args)...);
      }
    } else {
      return parseNonInlineTypes(variant, std::forward<Arg>(arg),
                                 std::forward<Args>(args)...);
    }
  }

  template<class FieldNameFn, class Variant, class... Args>
  auto doParseNonInlineTypes(FieldNameFn fieldName, Variant& variant,
                             Args&&... args) {
    auto loadVariant = [&]() -> Status {
      std::string_view type;
      auto data = Factory<ValueType>::create();
      auto res = this->self().parseVariantInformation(type, data, variant);
      if (!res.ok()) {
        return res;
      }
      auto parser = [this, data](auto& v) {
        auto inspector = make(data);
        return inspector.apply(v);
      };
      return parseType(type, parser, fieldName, variant.value,
                       std::forward<Args>(args)...);
    };
    auto& f = this->self();
    return f.beginObject()                     //
           | loadVariant                       //
           | [&]() { return f.endObject(); };  //
  }

  template<class... Ts, class... Args>
  auto parseNonInlineTypes(
      typename Base::template UnqualifiedVariant<Ts...>& variant,
      Args&&... args) {
    auto fieldName = [](auto const& arg) { return arg.tag; };
    return doParseNonInlineTypes(fieldName, variant,
                                 std::forward<Args>(args)...);
  }

  template<class... Ts, class... Args>
  auto parseNonInlineTypes(
      typename Base::template QualifiedVariant<Ts...>& variant,
      Args&&... args) {
    auto fieldName = [&variant](auto const& arg) { return variant.valueField; };
    return doParseNonInlineTypes(fieldName, variant,
                                 std::forward<Args>(args)...);
  }

  template<class... Ts, class... Args>
  auto parseNonInlineTypes(
      typename Base::template EmbeddedVariant<Ts...>& variant, Args&&... args) {
    auto& f = this->self();
    std::string_view type;
    auto loadVariant = [&]() -> Status {
      auto parser = [this, &variant, &f](auto& v) {
        return f.applyFields(this->ignoreField(variant.typeField),
                             this->embedFields(v));
      };
      return parseType(type, parser, std::monostate{}, variant.value,
                       std::forward<Args>(args)...);
    };
    return f.beginObject()                                               //
           | [&]() { return f.loadTypeField(variant.typeField, type); }  //
           | loadVariant                                                 //
           | [&]() { return f.endObject(); };                            //
  }

  Status::Success parseFields(FieldsMap&) { return Status::Success{}; }

  template<class Arg>
  Status parseFields(FieldsMap& fields, Arg&& arg) {
    return this->self().parseField(fields, std::forward<Arg>(arg));
  }

  template<class Arg, class... Args>
  Status parseFields(FieldsMap& fields, Arg&& arg, Args&&... args) {
    auto& f = this->self();
    return f.parseField(fields, std::forward<Arg>(arg)) |
           [&]() { return f.parseFields(fields, std::forward<Args>(args)...); };
  }

  [[nodiscard]] Status parseField(
      FieldsMap& fields,
      std::unique_ptr<detail::EmbeddedFields<Derived>>&& embeddedFields) {
    return embeddedFields->apply(this->self(), fields)  //
           | [&]() { return embeddedFields->checkInvariant(); };
  }

  [[nodiscard]] Status::Success parseField(FieldsMap& fields,
                                           typename Base::IgnoreField&& field) {
    if (auto it = fields.find(field.name); it != fields.end()) {
      assert(!it->second.second &&
             "field processed twice during inspection. Make sure field names "
             "are unique!");
      it->second.second = true;  // mark the field as processed
    }
    return {};
  }

  template<class T>
  [[nodiscard]] Status parseField(FieldsMap& fields, T&& field) {
    auto name = Base::getFieldName(field);
    bool isPresent = false;
    auto getFieldData = [&]() -> ValueType {
      if (auto it = fields.find(name); it != fields.end()) {
        assert(!it->second.second &&
               "field processed twice during inspection. Make sure field "
               "names "
               "are unique!");
        isPresent = true;
        it->second.second = true;  // mark the field as processed
        return it->second.first;
      }
      return {};
    };
    auto ff = make(getFieldData());
    auto& value = Base::getFieldValue(field);
    auto load = [&]() -> Status {
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

    auto res = load()                                            //
               | [&]() { return this->checkInvariant(field); };  //

    if (!res.ok()) {
      return {std::move(res), name, Status::AttributeTag{}};
    }
    return res;
  }

  template<class ParseFn, class FieldNameFn, class... Ts, class Arg,
           class... Args>
  Status parseType(std::string_view tag, ParseFn parse, FieldNameFn fieldName,
                   std::variant<Ts...>& result, Arg&& arg, Args&&... args) {
    static_assert(!Arg::isInlineType,
                  "All inline types must be listed at the beginning of the "
                  "alternatives list");
    if (arg.tag == tag) {
      auto v = Factory<typename Arg::Type>::create();
      auto res = parse(v);
      if (res.ok()) {
        result = v;
      } else if constexpr (!std::is_same_v<FieldNameFn, std::monostate>) {
        return {std::move(res), fieldName(arg), Status::AttributeTag{}};
      }
      return res;
    }

    if constexpr (sizeof...(Args) == 0) {
      return {"Found invalid type: " + std::string(tag)};
    } else {
      return parseType(tag, std::forward<ParseFn>(parse),
                       std::forward<FieldNameFn>(fieldName), result,
                       std::forward<Args>(args)...);
    }
  }

  template<class T>
  Status processMap(T& map) {
    return this->self().doProcessObject(
        [&](std::string_view key, ValueType value) -> Status {
          auto ff = this->make(value);
          auto val = Factory<typename T::mapped_type>::create();
          if (auto res = process(ff, val); !res.ok()) {
            return {std::move(res), "'" + std::string(key) + "'",
                    Status::ArrayTag{}};
          }
          map.emplace(key, std::move(val));
          return {};
        });
  }

  template<class T>
  Status processList(T& list) {
    constexpr auto isSet = detail::IsSetLike<T>::value;
    std::size_t idx = 0;
    return this->self().doProcessList([&](auto value) -> Status {
      auto ff = this->self().make(value);
      auto val = Factory<typename T::value_type>::create();
      if (auto res = process(ff, val); !res.ok()) {
        return {std::move(res), std::to_string(idx), Status::ArrayTag{}};
      }
      if constexpr (isSet) {
        list.insert(std::move(val));
      } else {
        list.push_back(std::move(val));
      }
      ++idx;
      return {};
    });
  }

  template<class T, size_t N>
  [[nodiscard]] Status processArray(T (&data)[N]) {
    std::size_t index = 0;
    return this->self().doProcessList([&](auto value) -> Status {
      auto ff = this->self().make(value);
      if (auto res = process(ff, data[index]); !res.ok()) {
        return {std::move(res), std::to_string(index), Status::ArrayTag{}};
      }
      ++index;
      return {};
    });
  }

  template<class T, class U>
  Status checkInvariant(typename Base::template InvariantField<T, U>& field) {
    return Base::template doCheckInvariant<detail::FieldInvariantFailedError>(
        field.invariantFunc, Base::getFieldValue(field));
  }

  template<class T>
  auto checkInvariant(T& field) {
    if constexpr (!detail::IsRawField<std::remove_cvref_t<T>>::value) {
      return checkInvariant(field.inner);
    } else {
      return Status::Success{};
    }
  }

  ParseOptions _options;
};

}  // namespace arangodb::inspection

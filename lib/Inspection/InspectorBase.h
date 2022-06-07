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

#include <array>
#include <cstddef>
#include <functional>
#include <string_view>
#include <type_traits>
#include <variant>

#include <velocypack/Iterator.h>

#include "Inspection/Access.h"
#include "Inspection/Types.h"

namespace arangodb::inspection {

#ifdef _MSC_VER
#define EMPTY_BASE __declspec(empty_bases)
#else
#define EMPTY_BASE
#endif

template<class Derived>
struct InspectorBase {
 protected:
  template<class T>
  struct Object;

  template<class... Ts>
  struct Variant;

  template<typename T>
  struct RawField;

  struct IgnoreField;

  struct Keep {};

  // TODO - support virtual fields with getter/setter
  // template<typename T>
  // struct VirtualField : BasicField<VirtualField<T>> {
  //   using value_type = T;
  // };

 public:
  template<class T>
  [[nodiscard]] Status apply(T& x) {
    return process(self(), x);
  }

  constexpr Keep keep() { return {}; }

  template<class T>
  [[nodiscard]] Object<T> object(T& o) noexcept {
    return Object<T>{self(), o};
  }

  template<class... Ts>
  [[nodiscard]] Variant<Ts...> variant(std::variant<Ts...>& v) noexcept {
    return Variant<Ts...>{self(), v};
  }

  template<typename T>
  [[nodiscard]] auto field(std::string_view name, T&& value) const noexcept {
    using TT = std::remove_cvref_t<T>;
    return field(name, static_cast<TT const&>(value));
  }

  template<typename T>
  [[nodiscard]] RawField<T> field(std::string_view name,
                                  T& value) const noexcept {
    static_assert(!std::is_const<T>::value || !Derived::isLoading,
                  "Loading inspector must pass non-const lvalue reference");
    return RawField<T>{{name}, value};
  }

  [[nodiscard]] IgnoreField ignoreField(std::string_view name) const noexcept {
    return IgnoreField{name};
  }

 protected:
  template<class InnerField, class Transformer>
  struct TransformField;

  template<class InnerField, class FallbackValue>
  struct FallbackField;

  Derived& self() { return static_cast<Derived&>(*this); }

  template<class T>
  struct IsRawField : std::false_type {};
  template<class T>
  struct IsRawField<RawField<T>> : std::true_type {};

  template<class T>
  struct IsTransformField : std::false_type {};
  template<class T, class U>
  struct IsTransformField<TransformField<T, U>> : std::true_type {};

  template<class T>
  struct IsFallbackField : std::false_type {};
  template<class T, class U>
  struct IsFallbackField<FallbackField<T, U>> : std::true_type {};

  template<class T>
  static std::string_view getFieldName(T& field) noexcept {
    if constexpr (IsRawField<std::remove_cvref_t<T>>::value ||
                  std::is_same_v<IgnoreField, std::remove_cvref_t<T>>) {
      return field.name;
    } else {
      return getFieldName(field.inner);
    }
  }

  template<class T>
  static auto& getFieldValue(T& field) noexcept {
    if constexpr (IsRawField<std::remove_cvref_t<T>>::value) {
      return field.value;
    } else {
      return getFieldValue(field.inner);
    }
  }

  template<class T>
  static decltype(auto) getFallbackField(T& field) noexcept {
    using TT = std::remove_cvref_t<T>;
    if constexpr (IsFallbackField<TT>::value) {
      return field;
    } else if constexpr (!IsRawField<TT>::value) {
      return getFallbackField(field.inner);
    }
  }

  template<class T>
  static decltype(auto) getTransformer(T& field) noexcept {
    using TT = std::remove_cvref_t<T>;
    if constexpr (IsTransformField<TT>::value) {
      auto& result = field.transformer;  // We want to return a reference!
      return result;
    } else if constexpr (!IsRawField<TT>::value) {
      return getTransformer(field.inner);
    }
  }

 private:
  template<class Field>
  struct EMPTY_BASE InvariantMixin {
    template<class Predicate>
    [[nodiscard]] auto invariant(Predicate predicate) && {
      return InvariantField<Field, Predicate>(
          std::move(static_cast<Field&>(*this)), std::move(predicate));
    }
  };

  template<class Field, class = void>
  struct HasInvariantMethod : std::false_type {};

  struct AlwaysTrue {
    template<class... Ts>
    [[nodiscard]] constexpr bool operator()(Ts&&...) const noexcept {
      return true;
    }
  };

  template<class Field>
  struct HasInvariantMethod<
      Field,
      std::void_t<decltype(std::declval<Field>().invariant(AlwaysTrue{}))>>
      : std::true_type {};

  template<class Inner>
  using WithInvariant =
      std::conditional_t<HasInvariantMethod<Inner>::value, std::monostate,
                         InvariantMixin<Inner>>;

  template<class Field>
  struct EMPTY_BASE FallbackMixin {
    template<class U>
    [[nodiscard]] auto fallback(U&& val) && {
      static_assert(std::is_constructible_v<typename Field::value_type, U> ||
                    std::is_same_v<Keep, U>);

      return FallbackField<Field, U>(std::move(static_cast<Field&>(*this)),
                                     std::forward<U>(val));
    }
  };

  template<class Field, class = void>
  struct HasFallbackMethod : std::false_type {};

  template<class Field>
  struct HasFallbackMethod<Field,
                           std::void_t<decltype(std::declval<Field>().fallback(
                               std::declval<typename Field::value_type>()))>>
      : std::true_type {};

  template<class Inner>
  using WithFallback = std::conditional_t<HasFallbackMethod<Inner>::value,
                                          std::monostate, FallbackMixin<Inner>>;

  template<class Field>
  struct EMPTY_BASE TransformMixin {
    template<class T>
    [[nodiscard]] auto transformWith(T transformer) && {
      return TransformField<Field, T>(std::move(static_cast<Field&>(*this)),
                                      std::move(transformer));
    }
  };

  template<class Field, class = void>
  struct HasTransformMethod : std::false_type {};

  template<class Field>
  struct HasTransformMethod<
      Field, std::void_t<decltype(std::declval<Field>().transformWith)>>
      : std::true_type {};

  template<class Inner>
  using WithTransform =
      std::conditional_t<HasTransformMethod<Inner>::value, std::monostate,
                         TransformMixin<Inner>>;

 protected:
  template<class InnerField, class FallbackValue>
  struct EMPTY_BASE FallbackField
      : Derived::template FallbackContainer<FallbackValue>,
        WithInvariant<FallbackField<InnerField, FallbackValue>>,
        WithTransform<FallbackField<InnerField, FallbackValue>> {
    FallbackField(InnerField inner, FallbackValue&& val)
        : Derived::template FallbackContainer<FallbackValue>(std::move(val)),
          inner(std::move(inner)) {}
    using value_type = typename InnerField::value_type;
    InnerField inner;
  };

  template<class InnerField, class Invariant>
  struct EMPTY_BASE InvariantField
      : Derived::template InvariantContainer<Invariant>,
        WithFallback<InvariantField<InnerField, Invariant>>,
        WithTransform<InvariantField<InnerField, Invariant>> {
    InvariantField(InnerField inner, Invariant&& invariant)
        : Derived::template InvariantContainer<Invariant>(std::move(invariant)),
          inner(std::move(inner)) {}
    using value_type = typename InnerField::value_type;
    InnerField inner;
  };

  template<class InnerField, class Transformer>
  struct EMPTY_BASE TransformField
      : WithInvariant<TransformField<InnerField, Transformer>>,
        WithFallback<TransformField<InnerField, Transformer>> {
    TransformField(InnerField inner, Transformer&& transformer)
        : inner(std::move(inner)), transformer(std::move(transformer)) {}
    using value_type = typename InnerField::value_type;
    InnerField inner;
    Transformer transformer;
  };

  template<typename DerivedField>
  struct EMPTY_BASE BasicField : InvariantMixin<DerivedField>,
                                 FallbackMixin<DerivedField>,
                                 TransformMixin<DerivedField> {
    explicit BasicField(std::string_view name) : name(name) {}
    std::string_view name;
  };

  template<typename T>
  struct EMPTY_BASE RawField : BasicField<RawField<T>> {
    RawField(std::string_view name, T& value)
        : BasicField<RawField>(name), value(value) {}
    using value_type = T;
    T& value;
  };

  struct IgnoreField {
    explicit IgnoreField(std::string_view name) : name(name) {}
    std::string_view name;
  };

  template<class T>
  struct FieldsResult {
    template<class Invariant>
    Status invariant(Invariant&& func) {
      if constexpr (Derived::isLoading) {
        if (!result.ok()) {
          return std::move(result);
        }
        return checkInvariant<FieldsResult, FieldsResult::InvariantFailedError>(
            std::forward<Invariant>(func), object);
      } else {
        return std::move(result);
      }
    }
    operator Status() && { return std::move(result); }

    static constexpr const char InvariantFailedError[] =
        "Object invariant failed";

   private:
    template<class TT>
    friend struct Object;
    FieldsResult(Status&& res, T& object)
        : result(std::move(res)), object(object) {}
    Status result;
    T& object;
  };

  template<class T>
  struct Object {
    template<class... Args>
    [[nodiscard]] FieldsResult<T> fields(Args&&... args) {
      auto& i = inspector;
      auto res =
          i.beginObject()                                                 //
          | [&]() { return i.applyFields(std::forward<Args>(args)...); }  //
          | [&]() { return i.endObject(); };                              //

      return {std::move(res), object};
    }

   private:
    friend struct InspectorBase;
    explicit Object(Derived& inspector, T& o)
        : inspector(inspector), object(o) {}

    Derived& inspector;
    T& object;
  };

  template<template<class...> class DerivedVariant, class... Ts>
  struct VariantBase {
    VariantBase(Derived& inspector, std::variant<Ts...>& value)
        : inspector(inspector), value(value) {}

    template<class... Args>
    auto alternatives(Args&&... args) && {
      // TODO - check that args cover all types and that there are no duplicates
      return inspector.processVariant(
          static_cast<DerivedVariant<Ts...>&>(*this),
          std::forward<Args>(args)...);
    }

    Derived& inspector;
    std::variant<Ts...>& value;
  };

  template<class... Ts>
  struct UnqualifiedVariant : VariantBase<UnqualifiedVariant, Ts...> {
    using VariantBase<UnqualifiedVariant, Ts...>::VariantBase;
  };

  template<class... Ts>
  struct QualifiedVariant : VariantBase<QualifiedVariant, Ts...> {
    QualifiedVariant(Derived& inspector, std::variant<Ts...>& value,
                     std::string_view typeField, std::string_view valueField)
        : VariantBase<QualifiedVariant, Ts...>(inspector, value),
          typeField(typeField),
          valueField(valueField) {}

    std::string_view const typeField;
    std::string_view const valueField;
  };

  template<class... Ts>
  struct Variant {
    UnqualifiedVariant<Ts...> unqualified() && {
      return UnqualifiedVariant<Ts...>(_inspector, _value);
    }

    QualifiedVariant<Ts...> qualified(std::string_view typeField,
                                      std::string_view valueField) && {
      return QualifiedVariant<Ts...>(_inspector, _value, typeField, valueField);
    }

   private:
    friend struct InspectorBase;
    Variant(Derived& inspector, std::variant<Ts...>& value)
        : _inspector(inspector), _value(value) {}
    Derived& _inspector;
    std::variant<Ts...>& _value;
  };

  template<class T, const char ErrorMsg[], class Func, class... Args>
  static Status checkInvariant(Func&& func, Args&&... args) {
    using result_t = std::invoke_result_t<Func, Args...>;
    if constexpr (std::is_same_v<result_t, bool>) {
      if (!std::invoke(std::forward<Func>(func), std::forward<Args>(args)...)) {
        return {ErrorMsg};
      }
      return {};
    } else {
      static_assert(std::is_same_v<result_t, Status>,
                    "Invariants must either return bool or "
                    "velocypack::inspection::Status");
      return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
    }
  }
};

#undef EMPTY_BASE

}  // namespace arangodb::inspection

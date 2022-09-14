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
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <ucontext.h>
#include <utility>
#include <variant>

#include <velocypack/Iterator.h>

#include "Inspection/Access.h"
#include "Inspection/Types.h"
#include "Inspection/detail/Fields.h"

namespace arangodb::inspection {

#ifdef _MSC_VER
#define EMPTY_BASE __declspec(empty_bases)
#else
#define EMPTY_BASE
#endif

struct NoContext {};

namespace detail {
template<class ContextT>
struct ContextContainer {
  static constexpr bool hasContext = true;
  using Context = ContextT;
  explicit ContextContainer(Context const& ctx) : _context(ctx) {}
  Context const& getContext() const noexcept { return _context; }

 private:
  Context const& _context;
};

template<>
struct EMPTY_BASE ContextContainer<NoContext> {
  static constexpr bool hasContext = false;
  ContextContainer() = default;
  explicit ContextContainer(NoContext const&) {}
};
}  // namespace detail

template<class Derived, class Context, class TargetInspector = Derived>
struct InspectorBase : detail::ContextContainer<Context> {
 protected:
  template<class T>
  struct Object;

  template<class... Ts>
  struct Variant;

  // TODO - support virtual fields with getter/setter
  // template<typename T>
  // struct VirtualField : BasicField<VirtualField<T>> {
  //   using value_type = T;
  // };

  using Keep = detail::Keep;

  using IgnoreField = detail::IgnoreField;

  template<class T>
  using RawField = detail::RawField<TargetInspector, T>;

  template<class InnerField, class Transformer>
  using TransformField =
      detail::TransformField<TargetInspector, InnerField, Transformer>;

  template<class InnerField, class FallbackValue>
  using FallbackField =
      detail::FallbackField<TargetInspector, InnerField, FallbackValue>;

  template<class InnerField, class FallbackFactory>
  using FallbackFactoryField =
      detail::FallbackFactoryField<TargetInspector, InnerField,
                                   FallbackFactory>;

  template<class InnerField, class Invariant>
  using InvariantField =
      detail::InvariantField<TargetInspector, InnerField, Invariant>;

 public:
  InspectorBase() = default;

  explicit InspectorBase(Context const& ctx)
      : detail::ContextContainer<Context>(ctx) {}

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

  template<class T>
  [[nodiscard]] auto embedFields(T& value) const;

  [[nodiscard]] IgnoreField ignoreField(std::string_view name) const noexcept {
    return IgnoreField{name};
  }

 protected:
  Derived& self() { return static_cast<Derived&>(*this); }

  template<class T>
  static std::string_view getFieldName(T& field) noexcept {
    if constexpr (detail::IsRawField<std::remove_cvref_t<T>>::value ||
                  std::is_same_v<IgnoreField, std::remove_cvref_t<T>>) {
      return field.name;
    } else {
      return getFieldName(field.inner);
    }
  }

  template<class T>
  static auto& getFieldValue(T& field) noexcept {
    if constexpr (detail::IsRawField<std::remove_cvref_t<T>>::value) {
      return field.value;
    } else {
      return getFieldValue(field.inner);
    }
  }

  template<class T>
  static decltype(auto) getFallbackField(T& field) noexcept {
    using TT = std::remove_cvref_t<T>;
    if constexpr (detail::IsFallbackField<TT>::value) {
      return field;
    } else if constexpr (!detail::IsRawField<TT>::value) {
      return getFallbackField(field.inner);
    }
  }

  template<class T>
  static decltype(auto) getTransformer(T& field) noexcept {
    using TT = std::remove_cvref_t<T>;
    if constexpr (detail::IsTransformField<TT>::value) {
      auto& result = field.transformer;  // We want to return a reference!
      return result;
    } else if constexpr (!detail::IsRawField<TT>::value) {
      return getTransformer(field.inner);
    }
  }

 protected:
  template<class T>
  struct FieldsResult {
    template<class Invariant>
    auto invariant(Invariant&& func) {
      return inspector.objectInvariant(object, std::forward<Invariant>(func),
                                       std::move(result));
    }

    operator Status() && { return std::move(result); }

   private:
    template<class TT>
    friend struct Object;
    FieldsResult(Status&& res, T& object, Derived& inspector)
        : result(std::move(res)), object(object), inspector(inspector) {}
    Status result;
    T& object;
    Derived& inspector;
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

      return {std::move(res), object, inspector};
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

  template<const char ErrorMsg[], class Func, class... Args>
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

  template<class, class, class>
  friend struct detail::EmbeddedFieldsWithObjectInvariant;
};

namespace detail {
template<class Parent, class Context>
struct EmbeddedFieldInspector
    : InspectorBase<EmbeddedFieldInspector<Parent, Context>, Context, Parent> {
  using Base =
      InspectorBase<EmbeddedFieldInspector<Parent, Context>, Context, Parent>;

  static constexpr bool isLoading = Parent::isLoading;

  [[nodiscard]] Status::Success beginObject() { return {}; }

  [[nodiscard]] Status::Success endObject() { return {}; }

  template<class T>
  auto value(T&) {
    return fail();
  }

  auto beginArray() { return fail(); }

  auto endArray() { return fail(); }

  template<class T>
  auto list(T& list) {
    return fail();
  }

  template<class T>
  auto map(T& map) {
    return fail();
  }

  template<class T>
  auto tuple(T& data) {
    return fail();
  }

  template<class... Args>
  Status applyFields(Args&&... args) {
    fields = std::make_unique<EmbeddedFieldsImpl<Parent, Args...>>(
        std::forward<Args>(args)...);
    return {};
  }

  template<class... Ts, class... Args>
  auto processVariant(
      typename Base::template UnqualifiedVariant<Ts...>& variant,
      Args&&... args) {
    return fail();
  }

  template<class... Ts, class... Args>
  auto processVariant(typename Base::template QualifiedVariant<Ts...>& variant,
                      Args&&... args) {
    return fail();
  }

  template<class Fn, class T>
  Status objectInvariant(T& object, Fn&& invariant, Status result) {
    TRI_ASSERT(result.ok())
        << "embedded inspection failed with error " << result.error();
    fields = std::make_unique<EmbeddedFieldsWithObjectInvariant<Parent, T, Fn>>(
        object, std::forward<Fn>(invariant), std::move(fields));
    return result;
  }

  std::unique_ptr<EmbeddedFields<Parent>> fields;

 private:
  template<bool Fail = true>
  Status::Success fail() {
    static_assert(!Fail, "embedFields can only be used for objects");
    return {};
  }
};

}  // namespace detail

template<class Derived, class Context, class TargetInspector>
template<class T>
auto InspectorBase<Derived, Context, TargetInspector>::embedFields(
    T& value) const {
  auto insp = [this]() {
    if constexpr (InspectorBase::hasContext) {
      return detail::EmbeddedFieldInspector<Derived, Context>(
          this->getContext());
    } else {
      return detail::EmbeddedFieldInspector<Derived, Context>();
    }
  }();
  auto res = insp.apply(value);
  TRI_ASSERT(res.ok());
  return std::move(insp.fields);
}

#undef EMPTY_BASE

}  // namespace arangodb::inspection

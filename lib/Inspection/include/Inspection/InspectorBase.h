////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include <utility>
#include <variant>

#include <velocypack/Iterator.h>

#include "Inspection/Access.h"
#include "Inspection/Types.h"
#include "Inspection/detail/Fields.h"

namespace arangodb::inspection {

struct NoContext {};

namespace detail {
struct NoOp {
  template<class T>
  constexpr void operator()(T& v) const noexcept {}
};
}  // namespace detail

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
struct ContextContainer<NoContext> {
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

  template<class T>
  struct Enum;

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

  template<class T>
  [[nodiscard]] Enum<T> enumeration(T& e) noexcept {
    static_assert(std::is_enum_v<T>);
    return Enum<T>{self(), e};
  }

  template<class... Ts>
  [[nodiscard]] Variant<Ts...> variant(std::variant<Ts...>& v) noexcept {
    return Variant<Ts...>{self(), v};
  }

  template<typename T>
  [[nodiscard]] auto field(std::string_view name, T&& value) const noexcept {
    static_assert(std::is_rvalue_reference_v<decltype(value)>);
    static_assert(!Derived::isLoading,
                  "Loading inspector must not pass rvalue reference");
    return RawField<T>{{name}, std::move(value)};
  }

  template<typename T>
  [[nodiscard]] RawField<T&> field(std::string_view name,
                                   T& value) const noexcept {
    static_assert(!std::is_const<T>::value || !Derived::isLoading,
                  "Loading inspector must pass non-const lvalue reference");
    return RawField<T&>{{name}, value};
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
    Object(Derived& inspector, T& o) : inspector(inspector), object(o) {}

    Derived& inspector;
    T& object;
  };

  template<class T>
  struct Enum {
    template<class Transformer, class... Args>
    [[nodiscard]] Status transformedValues(Transformer transformer,
                                           Args&&... args) {
      if constexpr (Derived::isLoading) {
        constexpr bool hasStringValues =
            (std::is_constructible_v<std::string, Args> || ...);
        constexpr bool hasIntValues =
            (std::is_constructible_v<std::uint64_t, Args> || ...);
        static_assert(
            hasStringValues || hasIntValues,
            "Enum values can only be mapped to string or unsigned values");
        Status res;
        bool retryDifferentType = false;
        if constexpr (hasStringValues) {
          static_assert(std::is_invocable_v<Transformer, std::string&>);
          // TODO - read std::string_view
          res = load<std::string>(transformer, retryDifferentType,
                                  std::forward<Args>(args)...);
          assert(retryDifferentType == false || !res.ok());
        }
        if constexpr (hasIntValues) {
          static_assert(std::is_invocable_v<Transformer, std::uint64_t&>);
          if (!hasStringValues || retryDifferentType) {
            retryDifferentType = false;
            res = load<std::uint64_t>(transformer, retryDifferentType,
                                      std::forward<Args>(args)...);
            if (hasStringValues && retryDifferentType) {
              return Status{"Expecting type String or Int"};
            }
          }
        }
        return res;
      } else {
        return store(std::forward<Args>(args)...);
      }
    }

    template<class... Args>
    [[nodiscard]] Status values(Args&&... args) {
      return transformedValues(detail::NoOp{}, std::forward<Args>(args)...);
    }

   private:
    friend struct InspectorBase;
    Enum(Derived& inspector, T& v) : _inspector(inspector), _value(v) {}

    template<class Arg, class... Args>
    static void checkType() {
      static_assert(
          std::is_constructible_v<std::string, Arg> || std::is_integral_v<Arg>,
          "Enum values can only be mapped to string or unsigned values");
    }

    template<class ValueType, class Transformer, class... Args>
    Status load(Transformer transformer, bool& retryDifferentType,
                Args&&... args) {
      ValueType read{};
      auto result = _inspector.apply(read);
      if (!result.ok()) {
        retryDifferentType = true;
        return result;
      }

      transformer(read);
      if (loadValue(read, std::forward<Args>(args)...)) {
        return Status{};
      } else if constexpr (std::is_integral_v<ValueType>) {
        return {"Unknown enum value " + std::to_string(read)};
      } else {
        return {"Unknown enum value " + read};
      }
    }

    template<class ValueType, class Arg, class... Args>
    bool loadValue(ValueType const& read, T v, Arg&& a, Args&&... args) {
      checkType<Arg>();
      if constexpr (std::is_constructible_v<ValueType, Arg>) {
        if (read == static_cast<ValueType>(a)) {
          this->_value = v;
          return true;
        }
      }

      if constexpr (sizeof...(args) > 0) {
        return loadValue(read, std::forward<Args>(args)...);
      } else {
        return false;
      }
    }

    template<class Arg, class... Args>
    Status store(T v, Arg&& a, Args&&... args) {
      checkType<Arg>();
      if (_value == v) {
        return _inspector.apply(convert(std::forward<Arg>(a)));
      }
      return store(std::forward<Args>(args)...);
    }

    Status store() {
      return {"Unknown enum value " +
              std::to_string(static_cast<std::size_t>(_value))};
    }

    template<std::size_t N>
    std::string_view convert(char const (&v)[N]) {
      return std::string_view(v, N - 1);
    }
    template<class TT>
    auto convert(TT&& v) {
      return std::forward<TT>(v);
    }

    Derived& _inspector;
    T& _value;
  };

  template<template<class...> class DerivedVariant, class... Ts>
  struct VariantBase {
    VariantBase(Derived& inspector, std::variant<Ts...>& value)
        : inspector(inspector), value(value) {}

    template<class... Args>
    auto alternatives(Args&&... args) && {
      // TODO - check that args cover all types and that there are no
      // duplicates
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
  struct EmbeddedVariant : VariantBase<EmbeddedVariant, Ts...> {
    EmbeddedVariant(Derived& inspector, std::variant<Ts...>& value,
                    std::string_view typeField)
        : VariantBase<EmbeddedVariant, Ts...>(inspector, value),
          typeField(typeField) {}

    std::string_view const typeField;
  };

  template<class... Ts>
  struct Variant {
    auto unqualified() && {
      return UnqualifiedVariant<Ts...>(_inspector, _value);
    }

    auto qualified(std::string_view typeField, std::string_view valueField) && {
      return QualifiedVariant<Ts...>(_inspector, _value, typeField, valueField);
    }

    auto embedded(std::string_view typeField) && {
      return EmbeddedVariant<Ts...>(_inspector, _value, typeField);
    }

   private:
    friend struct InspectorBase;
    Variant(Derived& inspector, std::variant<Ts...>& value)
        : _inspector(inspector), _value(value) {}
    Derived& _inspector;
    std::variant<Ts...>& _value;
  };

  template<const char ErrorMsg[], class Func, class... Args>
  static Status doCheckInvariant(Func&& func, Args&&... args) {
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

  explicit EmbeddedFieldInspector(Parent& parent) : Base(), _parent(parent) {}

  explicit EmbeddedFieldInspector(Parent& parent, Context const& context)
      : Base(context), _parent(parent) {}

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

  template<class... Ts, class... Args>
  auto processVariant(typename Base::template EmbeddedVariant<Ts...>& variant,
                      Args&&... args) {
    return fail();
  }

  template<class U>
  using FallbackContainer = typename Parent::template FallbackContainer<U>;

  template<class Fn>
  using FallbackFactoryContainer =
      typename Parent::template FallbackFactoryContainer<Fn>;

  template<class Invariant>
  using InvariantContainer =
      typename Parent::template InvariantContainer<Invariant>;

 private:
  template<class, class, class>
  friend struct ::arangodb::inspection::InspectorBase;

  using EmbeddedParam = typename Parent::EmbeddedParam;

  template<class... Args>
  auto processEmbeddedFields(EmbeddedParam& param, Args&&... args) {
    return _parent.processEmbeddedFields(param, std::forward<Args>(args)...);
  }

  template<class Fn, class T>
  Status objectInvariant(T& object, Fn&& invariant, Status result) {
    assert(result.ok());
    fields = std::make_unique<EmbeddedFieldsWithObjectInvariant<Parent, T, Fn>>(
        object, std::forward<Fn>(invariant), std::move(fields));
    return result;
  }

  Parent& parent() { return _parent; }

  template<bool Fail = true>
  Status::Success fail() {
    static_assert(!Fail, "embedFields can only be used for objects");
    return {};
  }

  std::unique_ptr<EmbeddedFields<Parent>> fields;

  Parent& _parent;
};

}  // namespace detail

template<class Derived, class Context, class TargetInspector>
template<class T>
auto InspectorBase<Derived, Context, TargetInspector>::embedFields(
    T& value) const {
  auto& parentInspector =
      [p = const_cast<InspectorBase*>(this)]() -> decltype(auto) {
    if constexpr (std::is_same_v<Derived, TargetInspector>) {
      return p->self();
    } else {
      // we are an embedded inspector
      return p->self().parent();
    }
  }();
  auto insp = [&]() {
    if constexpr (InspectorBase::hasContext) {
      return detail::EmbeddedFieldInspector<TargetInspector, Context>(
          parentInspector, this->getContext());
    } else {
      return detail::EmbeddedFieldInspector<TargetInspector, Context>(
          parentInspector);
    }
  }();
  auto res = insp.apply(value);
  assert(res.ok());
  return std::move(insp.fields);
}

}  // namespace arangodb::inspection

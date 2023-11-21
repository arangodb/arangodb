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

#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "Inspection/Status.h"

namespace arangodb::inspection::detail {

#ifdef _MSC_VER
#define EMPTY_BASE __declspec(empty_bases)
#else
#define EMPTY_BASE
#endif

struct Keep {};

struct IgnoreField;

template<class Inspector, class T>
struct RawField;

template<class Inspector, class InnerField, class Transformer>
struct TransformField;

template<class Inspector, class InnerField, class FallbackValue>
struct FallbackField;

template<class Inspector, class InnerField, class FallbackFactory>
struct FallbackFactoryField;

template<class Inspector, class InnerField, class Invariant>
struct InvariantField;

template<class Inspector, class Field>
struct EMPTY_BASE InvariantMixin {
  template<class Predicate>
  [[nodiscard]] auto invariant(Predicate predicate) && {
    return InvariantField<Inspector, Field, Predicate>(
        std::move(static_cast<Field&>(*this)), std::move(predicate));
  }
};

template<class Inspector, class Field>
struct EMPTY_BASE FallbackMixin {
  template<class U>
  [[nodiscard]] auto fallback(U&& val) && {
    static_assert(std::is_constructible_v<typename Field::value_type, U> ||
                  std::is_same_v<Keep, U>);

    return FallbackField<Inspector, Field, U>(
        std::move(static_cast<Field&>(*this)), std::forward<U>(val));
  }

  template<class Fn>
  [[nodiscard]] auto fallbackFactory(Fn&& fn) && {
    static_assert(std::is_constructible_v<typename Field::value_type,
                                          std::invoke_result_t<Fn>>);

    return FallbackFactoryField<Inspector, Field, Fn>(
        std::move(static_cast<Field&>(*this)), std::forward<Fn>(fn));
  }
};

template<class Inspector, class Field>
struct EMPTY_BASE TransformMixin {
  template<class T>
  [[nodiscard]] auto transformWith(T transformer) && {
    return TransformField<Inspector, Field, T>(
        std::move(static_cast<Field&>(*this)), std::move(transformer));
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
    Field, std::void_t<decltype(std::declval<Field>().invariant(AlwaysTrue{}))>>
    : std::true_type {};

template<class Inspector, class Inner>
using WithInvariant =
    std::conditional_t<HasInvariantMethod<Inner>::value, std::monostate,
                       InvariantMixin<Inspector, Inner>>;

template<class Field, class = void>
struct HasFallbackMethod : std::false_type {};

template<class Field>
struct HasFallbackMethod<Field,
                         std::void_t<decltype(std::declval<Field>().fallback(
                             std::declval<typename Field::value_type>()))>>
    : std::true_type {};

template<class Inspector, class Inner>
using WithFallback =
    std::conditional_t<HasFallbackMethod<Inner>::value, std::monostate,
                       FallbackMixin<Inspector, Inner>>;

template<class Field, class = void>
struct HasTransformMethod : std::false_type {};

template<class Field>
struct HasTransformMethod<
    Field, std::void_t<decltype(std::declval<Field>().transformWith)>>
    : std::true_type {};

template<class Inspector, class Inner>
using WithTransform =
    std::conditional_t<HasTransformMethod<Inner>::value, std::monostate,
                       TransformMixin<Inspector, Inner>>;

template<class Inspector, typename DerivedField>
struct EMPTY_BASE BasicField : InvariantMixin<Inspector, DerivedField>,
                               FallbackMixin<Inspector, DerivedField>,
                               TransformMixin<Inspector, DerivedField> {
  explicit BasicField(std::string_view name) : name(name) {}
  std::string_view name;
};

template<class Inspector, typename T>
struct EMPTY_BASE RawField : BasicField<Inspector, RawField<Inspector, T>> {
  template<class TT>
  RawField(std::string_view name, TT&& value)
      : BasicField<Inspector, RawField>(name), value(std::forward<TT>(value)) {}
  using value_type = std::remove_reference_t<T>;
  T value;
};

struct IgnoreField {
  explicit IgnoreField(std::string_view name) : name(name) {}
  std::string_view name;
};

template<class Inspector, class InnerField, class FallbackValue>
struct EMPTY_BASE FallbackField
    : Inspector::template FallbackContainer<FallbackValue>,
      WithInvariant<Inspector,
                    FallbackField<Inspector, InnerField, FallbackValue>>,
      WithTransform<Inspector,
                    FallbackField<Inspector, InnerField, FallbackValue>> {
  FallbackField(InnerField inner, FallbackValue&& val)
      : Inspector::template FallbackContainer<FallbackValue>(std::move(val)),
        inner(std::move(inner)) {}
  using value_type = typename InnerField::value_type;
  InnerField inner;
};

template<class Inspector, class InnerField, class FallbackFactory>
struct EMPTY_BASE FallbackFactoryField
    : Inspector::template FallbackFactoryContainer<FallbackFactory>,
      WithInvariant<Inspector, FallbackFactoryField<Inspector, InnerField,
                                                    FallbackFactory>>,
      WithTransform<Inspector, FallbackFactoryField<Inspector, InnerField,
                                                    FallbackFactory>> {
  FallbackFactoryField(InnerField inner, FallbackFactory&& fn)
      : Inspector::template FallbackFactoryContainer<FallbackFactory>(
            std::move(fn)),
        inner(std::move(inner)) {}
  using value_type = typename InnerField::value_type;
  InnerField inner;
};

template<class Inspector, class InnerField, class Invariant>
struct EMPTY_BASE InvariantField
    : Inspector::template InvariantContainer<Invariant>,
      WithFallback<Inspector, InvariantField<Inspector, InnerField, Invariant>>,
      WithTransform<Inspector,
                    InvariantField<Inspector, InnerField, Invariant>> {
  InvariantField(InnerField inner, Invariant&& invariant)
      : Inspector::template InvariantContainer<Invariant>(std::move(invariant)),
        inner(std::move(inner)) {}
  using value_type = typename InnerField::value_type;
  InnerField inner;
};

template<class Inspector, class InnerField, class Transformer>
struct EMPTY_BASE TransformField
    : WithInvariant<Inspector,
                    TransformField<Inspector, InnerField, Transformer>>,
      WithFallback<Inspector,
                   TransformField<Inspector, InnerField, Transformer>> {
  TransformField(InnerField inner, Transformer&& transformer)
      : inner(std::move(inner)), transformer(std::move(transformer)) {}
  using value_type = typename InnerField::value_type;
  InnerField inner;
  Transformer transformer;
};

#undef EMPTY_BASE

template<class T>
struct IsRawField : std::false_type {};
template<class Inspector, class T>
struct IsRawField<RawField<Inspector, T>> : std::true_type {};

template<class T>
struct IsTransformField : std::false_type {};
template<class Inspector, class T, class U>
struct IsTransformField<TransformField<Inspector, T, U>> : std::true_type {};

template<class T>
struct IsFallbackField : std::false_type {};
template<class Inspector, class T, class U>
struct IsFallbackField<FallbackField<Inspector, T, U>> : std::true_type {};
template<class Inspector, class T, class Fn>
struct IsFallbackField<FallbackFactoryField<Inspector, T, Fn>>
    : std::true_type {};

static constexpr const char FieldInvariantFailedError[] =
    "Field invariant failed";

static constexpr const char ObjectInvariantFailedError[] =
    "Object invariant failed";

template<class Inspector>
struct EmbeddedFields {
  virtual ~EmbeddedFields() = default;
  virtual Status apply(Inspector&, typename Inspector::EmbeddedParam&) = 0;
  virtual Status checkInvariant() { return {}; };
};

template<class Inspector, class... Ts>
struct EmbeddedFieldsImpl : EmbeddedFields<Inspector> {
  template<class... Args>
  explicit EmbeddedFieldsImpl(Args&&... args)
      : fields(std::forward<Args>(args)...) {}

  Status apply(Inspector& inspector,
               typename Inspector::EmbeddedParam& param) override {
    return std::invoke(
        [&]<std::size_t... I>(std::index_sequence<I...>) {
          return inspector.processEmbeddedFields(
              param, std::get<I>(std::move(fields))...);
        },
        std::make_index_sequence<sizeof...(Ts)>{});
  }

 private:
  std::tuple<Ts...> fields;
};

template<class Inspector, class Object, class Invariant>
struct EmbeddedFieldsWithObjectInvariant : EmbeddedFields<Inspector> {
  template<class Fn>
  explicit EmbeddedFieldsWithObjectInvariant(
      Object& object, Fn&& invariant,
      std::unique_ptr<EmbeddedFields<Inspector>> fields)
      : fields(std::move(fields)),
        invariant(std::forward<Fn>(invariant)),
        object(object) {}

  Status apply(Inspector& inspector,
               typename Inspector::EmbeddedParam& param) override {
    return fields->apply(inspector, param);
  }

  Status checkInvariant() override {
    return Inspector::Base::template doCheckInvariant<
        detail::ObjectInvariantFailedError>(std::forward<Invariant>(invariant),
                                            object);
  }

 private:
  std::unique_ptr<EmbeddedFields<Inspector>> fields;
  Invariant invariant;
  Object& object;
};

}  // namespace arangodb::inspection::detail

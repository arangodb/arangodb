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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "Inspection/Status.h"
#include "Inspection/Types.h"

namespace arangodb::inspection::detail {

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

/// Determines for which inspection direction a field condition is evaluated.
/// In the direction it does not apply to, the field is always processed.
enum class ConditionScope { Always, Loading, Saving };

template<class Inspector, class InnerField, class Predicate,
         ConditionScope Scope>
struct ConditionalField;

template<class T>
struct IsConditionalField : std::false_type {};
template<class Inspector, class T, class P, ConditionScope S>
struct IsConditionalField<ConditionalField<Inspector, T, P, S>>
    : std::true_type {};

/// Evaluates `predicate` if `Scope` applies to the direction `Inspector` runs
/// in; otherwise the field takes part as usual.
template<class Inspector, ConditionScope Scope, class Predicate>
FieldCondition evaluateScopedCondition(Predicate const& predicate) {
  constexpr bool applies =
      Scope == ConditionScope::Always ||
      (Inspector::isLoading ? Scope == ConditionScope::Loading
                            : Scope == ConditionScope::Saving);
  if constexpr (applies) {
    return std::invoke(predicate);
  } else {
    return FieldCondition::Process;
  }
}

/// True if `Field` or any of the fields it wraps carries a condition.
template<class Field, class = void>
struct ContainsCondition : std::false_type {};

template<class Field>
struct ContainsCondition<Field, std::void_t<decltype(Field::inner)>>
    : std::disjunction<
          IsConditionalField<Field>,
          ContainsCondition<std::remove_cvref_t<decltype(Field::inner)>>> {};

template<class Inspector, class Field>
struct InvariantMixin {
  template<class Predicate>
  [[nodiscard]] auto invariant(Predicate predicate) && {
    return InvariantField<Inspector, Field, Predicate>(
        std::move(static_cast<Field&>(*this)), std::move(predicate));
  }
};

template<class Inspector, class Field>
struct FallbackMixin {
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
struct TransformMixin {
  template<class T>
  [[nodiscard]] auto transformWith(T transformer) && {
    return TransformField<Inspector, Field, T>(
        std::move(static_cast<Field&>(*this)), std::move(transformer));
  }
};

/// Attaches a condition to a field. A field carries at most one condition, so
/// `ConditionalField` deliberately does not inherit this mixin - chaining two
/// conditions is a compile error instead of one of them being dropped.
template<class Inspector, class Field>
struct ConditionMixin {
  /// Evaluates `predicate` both when loading and when saving.
  template<class Predicate>
  [[nodiscard]] auto when(Predicate predicate) && {
    return makeConditional<ConditionScope::Always>(std::move(predicate));
  }

  /// Evaluates `predicate` when loading; the field is always saved.
  template<class Predicate>
  [[nodiscard]] auto whenLoading(Predicate predicate) && {
    return makeConditional<ConditionScope::Loading>(std::move(predicate));
  }

  /// Evaluates `predicate` when saving; the field is always loaded.
  template<class Predicate>
  [[nodiscard]] auto whenSaving(Predicate predicate) && {
    return makeConditional<ConditionScope::Saving>(std::move(predicate));
  }

 private:
  template<ConditionScope Scope, class Predicate>
  auto makeConditional(Predicate&& predicate) {
    static_assert(
        std::is_invocable_r_v<FieldCondition, Predicate> &&
            std::is_same_v<std::invoke_result_t<Predicate>, FieldCondition>,
        "Field conditions must be invocable without arguments and return "
        "inspection::FieldCondition");
    static_assert(!ContainsCondition<Field>::value,
                  "A field must not carry more than one condition");

    return ConditionalField<Inspector, Field, Predicate, Scope>(
        std::move(static_cast<Field&>(*this)),
        std::forward<Predicate>(predicate));
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
struct BasicField : InvariantMixin<Inspector, DerivedField>,
                    FallbackMixin<Inspector, DerivedField>,
                    TransformMixin<Inspector, DerivedField>,
                    ConditionMixin<Inspector, DerivedField> {
  explicit BasicField(std::string_view name) : name(name) {}
  std::string_view name;
};

template<class Inspector, typename T>
struct RawField : BasicField<Inspector, RawField<Inspector, T>> {
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
struct FallbackField
    : Inspector::template FallbackContainer<FallbackValue>,
      WithInvariant<Inspector,
                    FallbackField<Inspector, InnerField, FallbackValue>>,
      WithTransform<Inspector,
                    FallbackField<Inspector, InnerField, FallbackValue>>,
      ConditionMixin<Inspector,
                     FallbackField<Inspector, InnerField, FallbackValue>> {
  FallbackField(InnerField inner, FallbackValue&& val)
      : Inspector::template FallbackContainer<FallbackValue>(std::move(val)),
        inner(std::move(inner)) {}
  using value_type = typename InnerField::value_type;
  InnerField inner;
};

template<class Inspector, class InnerField, class FallbackFactory>
struct FallbackFactoryField
    : Inspector::template FallbackFactoryContainer<FallbackFactory>,
      WithInvariant<Inspector, FallbackFactoryField<Inspector, InnerField,
                                                    FallbackFactory>>,
      WithTransform<Inspector, FallbackFactoryField<Inspector, InnerField,
                                                    FallbackFactory>>,
      ConditionMixin<Inspector, FallbackFactoryField<Inspector, InnerField,
                                                     FallbackFactory>> {
  FallbackFactoryField(InnerField inner, FallbackFactory&& fn)
      : Inspector::template FallbackFactoryContainer<FallbackFactory>(
            std::move(fn)),
        inner(std::move(inner)) {}
  using value_type = typename InnerField::value_type;
  InnerField inner;
};

template<class Inspector, class InnerField, class Invariant>
struct InvariantField
    : Inspector::template InvariantContainer<Invariant>,
      WithFallback<Inspector, InvariantField<Inspector, InnerField, Invariant>>,
      WithTransform<Inspector,
                    InvariantField<Inspector, InnerField, Invariant>>,
      ConditionMixin<Inspector,
                     InvariantField<Inspector, InnerField, Invariant>> {
  InvariantField(InnerField inner, Invariant&& invariant)
      : Inspector::template InvariantContainer<Invariant>(std::move(invariant)),
        inner(std::move(inner)) {}
  using value_type = typename InnerField::value_type;
  InnerField inner;
};

template<class Inspector, class InnerField, class Transformer>
struct TransformField
    : WithInvariant<Inspector,
                    TransformField<Inspector, InnerField, Transformer>>,
      WithFallback<Inspector,
                   TransformField<Inspector, InnerField, Transformer>>,
      ConditionMixin<Inspector,
                     TransformField<Inspector, InnerField, Transformer>> {
  TransformField(InnerField inner, Transformer&& transformer)
      : inner(std::move(inner)), transformer(std::move(transformer)) {}
  using value_type = typename InnerField::value_type;
  InnerField inner;
  Transformer transformer;
};

template<class Inspector, class InnerField, class Predicate,
         ConditionScope Scope>
struct ConditionalField
    : WithInvariant<Inspector,
                    ConditionalField<Inspector, InnerField, Predicate, Scope>>,
      WithFallback<Inspector,
                   ConditionalField<Inspector, InnerField, Predicate, Scope>>,
      WithTransform<Inspector,
                    ConditionalField<Inspector, InnerField, Predicate, Scope>> {
  ConditionalField(InnerField inner, Predicate&& predicate)
      : inner(std::move(inner)), predicate(std::move(predicate)) {}
  using value_type = typename InnerField::value_type;
  static constexpr ConditionScope scope = Scope;
  InnerField inner;
  Predicate predicate;
};

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

/// Absence of a condition on an embedded group.
struct NoCondition {};

/// `embedFields` returns simply a reference to the object whose fields are to
/// be spliced into the enclosing one. The nested `inspect` runs later, when the
/// enclosing inspector actually processes this entry.
/// A condition may be attached, which then governs the group as a whole.
template<class T, class Predicate = NoCondition,
         ConditionScope Scope = ConditionScope::Always>
struct EmbeddedFieldsRef {
  static constexpr bool hasCondition = !std::is_same_v<Predicate, NoCondition>;
  static constexpr ConditionScope scope = Scope;

  T& value;
  [[no_unique_address]] Predicate predicate{};

  template<class P>
      [[nodiscard]] auto when(P predicate) && requires(!hasCondition) {
    return makeConditional<ConditionScope::Always>(std::move(predicate));
  }

  template<class P>
      [[nodiscard]] auto whenLoading(P predicate) && requires(!hasCondition) {
    return makeConditional<ConditionScope::Loading>(std::move(predicate));
  }

  template<class P>
      [[nodiscard]] auto whenSaving(P predicate) && requires(!hasCondition) {
    return makeConditional<ConditionScope::Saving>(std::move(predicate));
  }

 private :
     // need a comment here to work around stupid clang-format behavior
     template<ConditionScope S, class P>
     auto
     makeConditional(P&& predicate) {
    static_assert(
        std::is_invocable_r_v<FieldCondition, P> &&
            std::is_same_v<std::invoke_result_t<P>, FieldCondition>,
        "Field conditions must be invocable without arguments and return "
        "inspection::FieldCondition");

    return EmbeddedFieldsRef<T, P, S>{value, std::forward<P>(predicate)};
  }
};

template<class T>
struct IsEmbeddedFieldsRef : std::false_type {};
template<class T, class P, ConditionScope S>
struct IsEmbeddedFieldsRef<EmbeddedFieldsRef<T, P, S>> : std::true_type {};

/// The group takes part unless a condition applying to this direction says
/// otherwise.
template<class Inspector, class T, class P, ConditionScope S>
FieldCondition embeddedFieldsCondition(EmbeddedFieldsRef<T, P, S> const& ref) {
  if constexpr (std::is_same_v<P, NoCondition>) {
    return FieldCondition::Process;
  } else {
    return evaluateScopedCondition<Inspector, S>(ref.predicate);
  }
}

}  // namespace arangodb::inspection::detail

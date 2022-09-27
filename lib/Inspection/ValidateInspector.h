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

#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "Inspection/InspectorBase.h"
#include "Inspection/Status.h"

namespace arangodb::inspection {

template<class Context = NoContext>
struct ValidateInspector : InspectorBase<ValidateInspector<Context>, Context> {
 protected:
  using Base = InspectorBase<ValidateInspector, Context>;

 public:
  static constexpr bool isLoading = true;

  ValidateInspector() requires(!Base::hasContext) = default;
  ValidateInspector(Context const& context) : Base(context) {}

  template<class T>
  [[nodiscard]] Status::Success value(T&) {
    return {};
  }

  [[nodiscard]] Status::Success beginObject() { return {}; }

  [[nodiscard]] Status::Success endObject() { return {}; }

  [[nodiscard]] Status::Success beginArray() { return {}; }

  [[nodiscard]] Status::Success endArray() { return {}; }

  template<class T>
  [[nodiscard]] Status::Success list(T& list) {
    return {};
  }

  template<class T>
  [[nodiscard]] Status::Success map(T& map) {
    return {};
  }

  template<class T>
  [[nodiscard]] Status::Success tuple(T& data) {
    return {};
  }

  template<class T, size_t N>
  [[nodiscard]] Status::Success tuple(T (&data)[N]) {
    return {};
  }

  template<class U>
  struct FallbackContainer {
    explicit FallbackContainer(U&&) {}
  };
  template<class Fn>
  struct FallbackFactoryContainer {
    explicit FallbackFactoryContainer(Fn&&) {}
  };

  template<class Invariant>
  struct InvariantContainer {
    explicit InvariantContainer(Invariant&& invariant)
        : invariantFunc(std::move(invariant)) {}
    Invariant invariantFunc;
  };

  template<class... Args>
  Status applyFields(Args&&... args) {
    return validateFields(std::forward<Args>(args)...);
  }

  template<class... Ts, class... Args>
  Status::Success processVariant(
      typename Base::template UnqualifiedVariant<Ts...>& variant,
      Args&&... args) {
    return {};
  }

  template<class... Ts, class... Args>
  Status::Success processVariant(
      typename Base::template QualifiedVariant<Ts...>& variant,
      Args&&... args) {
    return {};
  }

  template<class Invariant, class T>
  Status objectInvariant(T& object, Invariant&& func, Status result) {
    if (result.ok()) {
      result =
          Base::template checkInvariant<detail::ObjectInvariantFailedError>(
              std::forward<Invariant>(func), object);
    }
    return result;
  }

 private:
  template<class>
  friend struct detail::EmbeddedFields;
  template<class, class...>
  friend struct detail::EmbeddedFieldsImpl;
  template<class, class, class>
  friend struct detail::EmbeddedFieldsWithObjectInvariant;
  template<class, class>
  friend struct detail::EmbeddedFieldInspector;

  using EmbeddedParam = std::monostate;

  template<class... Args>
  Status processEmbeddedFields(EmbeddedParam&, Args&&... args) {
    return validateFields(std::forward<Args>(args)...);
  }

  Status::Success validateFields() { return Status::Success{}; }

  template<class Arg>
  Status validateFields(Arg&& arg) {
    return validateField(std::forward<Arg>(arg));
  }

  template<class Arg, class... Args>
  Status validateFields(Arg&& arg, Args&&... args) {
    return validateField(std::forward<Arg>(arg)) |
           [&]() { return validateFields(std::forward<Args>(args)...); };
  }

  [[nodiscard]] Status validateField(
      std::unique_ptr<detail::EmbeddedFields<ValidateInspector>>&&
          embeddedFields) {
    EmbeddedParam params;
    return embeddedFields->apply(*this, params)  //
           | [&]() { return embeddedFields->checkInvariant(); };
  }

  template<class T>
  [[nodiscard]] Status validateField(T&& field) {
    auto name = Base::getFieldName(field);
    auto& value = Base::getFieldValue(field);

    auto res = loadField(*this, name, true, value)         //
               | [&]() { return checkInvariant(field); };  //

    if (!res.ok()) {
      return {std::move(res), name, Status::AttributeTag{}};
    }
    return res;
  }

  template<class T, class U>
  Status checkInvariant(typename Base::template InvariantField<T, U>& field) {
    return Base::template checkInvariant<detail::FieldInvariantFailedError>(
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
};

}  // namespace arangodb::inspection

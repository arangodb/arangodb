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
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

#include "Basics/overload.h"
#include "Inspection/InspectorBase.h"

namespace arangodb::inspection {

template<class Derived, class Context>
struct SaveInspectorBase : InspectorBase<Derived, Context> {
 protected:
  using Base = InspectorBase<Derived, Context>;

 public:
  static constexpr bool isLoading = false;

  SaveInspectorBase() requires(!Base::hasContext) = default;
  explicit SaveInspectorBase(Context const& context) : Base(context) {}

  template<class T>
  [[nodiscard]] Status apply(T const& x) {
    return process(this->self(), x);
  }

  template<class T>
  [[nodiscard]] auto tuple(T const& data) {
    auto& f = this->self();
    return f.beginArray()  //
           |
           [&]() {
             return f.template processTuple<0, std::tuple_size_v<T>>(data);
           }                                  //
           | [&]() { return f.endArray(); };  //
  }

  template<class T, size_t N>
  [[nodiscard]] auto tuple(T (&d)[N]) {
    auto& f = this->self();
    return f.beginArray()                                                 //
           | [&]() { return f.processList(std::begin(d), std::end(d)); }  //
           | [&]() { return f.endArray(); };                              //
  }

  template<class T>
  [[nodiscard]] auto list(T const& l) {
    auto& f = this->self();
    return f.beginArray()                                                 //
           | [&]() { return f.processList(std::begin(l), std::end(l)); }  //
           | [&]() { return f.endArray(); };                              //
  }

  template<class T>
  [[nodiscard]] auto map(T const& map) {
    auto& f = this->self();
    return f.beginObject()                        //
           | [&]() { return f.processMap(map); }  //
           | [&]() { return f.endObject(); };     //
  }

  auto applyFields() { return Status::Success{}; }

  template<class Arg, class... Args>
  auto applyFields(Arg&& arg, Args&&... args) {
    auto res = applyField(std::forward<Arg>(arg));
    if constexpr (sizeof...(args) == 0) {
      return res;
    } else {
      return std::move(res)                                                 //
             | [&]() { return applyFields(std::forward<Args>(args)...); };  //
    }
  }

  template<class... Ts, class... Args>
  auto processVariant(
      typename Base::template UnqualifiedVariant<Ts...>& variant,
      Args&&... args) {
    auto& f = this->self();
    return std::visit(
        overload{[&](typename Args::Type& v) {
          if constexpr (Args::isInlineType) {
            return f.apply(v);
          } else {
            return f.beginObject()                                       //
                   | [&]() { return applyField(f.field(args.tag, v)); }  //
                   | [&]() { return f.endObject(); };
          }
        }...},
        variant.value);
  }

  template<class... Ts, class... Args>
  auto processVariant(typename Base::template QualifiedVariant<Ts...>& variant,
                      Args&&... args) {
    auto& f = this->self();
    return std::visit(overload{[&](typename Args::Type& v) {
                        if constexpr (Args::isInlineType) {
                          return f.apply(v);
                        } else {
                          auto storeFields = [&]() {
                            return applyFields(
                                f.field(variant.typeField, args.tag),
                                f.field(variant.valueField, v));
                          };
                          return f.beginObject()  //
                                 | storeFields    //
                                 | [&]() { return f.endObject(); };
                        }
                      }...},
                      variant.value);
  }

  template<class... Ts, class... Args>
  auto processVariant(typename Base::template EmbeddedVariant<Ts...>& variant,
                      Args&&... args) {
    auto& f = this->self();
    return std::visit(overload{[&](typename Args::Type& v) {
                        if constexpr (Args::isInlineType) {
                          return f.apply(v);
                        } else {
                          auto storeFields = [&]() {
                            return applyFields(
                                f.field(variant.typeField, args.tag),
                                f.embedFields(v));
                          };
                          return f.beginObject()  //
                                 | storeFields    //
                                 | [&]() { return f.endObject(); };
                        }
                      }...},
                      variant.value);
  }

  template<class Fn, class T>
  Status objectInvariant(T& object, Fn&&, Status result) {
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

  template<class T, class... Args>
  auto processEmbeddedFields(T&, Args&&... args) {
    return this->applyFields(std::forward<Args>(args)...);
  }

  [[nodiscard]] auto applyField(
      std::unique_ptr<detail::EmbeddedFields<Derived>> const& fields) {
    typename Derived::EmbeddedParam param;
    return fields->apply(this->self(), param);
  }

  template<class T>
  [[nodiscard]] auto applyField(T const& field) {
    if constexpr (std::is_same_v<typename Base::IgnoreField, T>) {
      return Status::Success{};
    } else {
      auto name = Base::getFieldName(field);
      auto& value = Base::getFieldValue(field);
      constexpr bool hasFallback =
          !std::is_void_v<decltype(Base::getFallbackField(field))>;
      auto res = [&]() {
        if constexpr (!std::is_void_v<decltype(Base::getTransformer(field))>) {
          return saveTransformedField(this->self(), name, hasFallback, value,
                                      Base::getTransformer(field));
        } else {
          return saveField(this->self(), name, hasFallback, value);
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
      return process(this->self(), std::get<Idx>(data))  //
             | [&]() {
                 return this->self().template processTuple<Idx + 1, End>(data);
               };  //
    } else {
      return Status::Success{};
    }
  }

  template<class Iterator>
  [[nodiscard]] auto processList(Iterator begin, Iterator end)
      -> decltype(process(std::declval<Derived&>(), *begin)) {
    for (auto it = begin; it != end; ++it) {
      if (auto res = process(this->self(), *it); !res.ok()) {
        return res;
      }
    }
    return {};
  }
};

}  // namespace arangodb::inspection

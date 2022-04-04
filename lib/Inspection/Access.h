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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <velocypack/Value.h>

#include "Inspection/detail/traits.h"
#include "velocypack/Slice.h"

namespace arangodb::inspection {

template<class T>
struct Access;

template<class T>
struct AccessBase;

template<class T>
struct TypeTag;

template<>
struct TypeTag<std::monostate> {
  static constexpr std::string_view name() { return "monostate"; }
};

#define DEFINE_TYPE_TAG(type, tagname)                           \
  template<>                                                     \
  struct TypeTag<type> {                                         \
    static constexpr std::string_view name() { return tagname; } \
  };

DEFINE_TYPE_TAG(bool, "bool");
DEFINE_TYPE_TAG(float, "float");
DEFINE_TYPE_TAG(double, "double");
DEFINE_TYPE_TAG(std::string, "string");
DEFINE_TYPE_TAG(std::int8_t, "int8");
DEFINE_TYPE_TAG(std::int16_t, "int16");
DEFINE_TYPE_TAG(std::int32_t, "int32");
DEFINE_TYPE_TAG(std::int64_t, "int64");
DEFINE_TYPE_TAG(std::uint8_t, "uint8");
DEFINE_TYPE_TAG(std::uint16_t, "uint16");
DEFINE_TYPE_TAG(std::uint32_t, "uint32");
DEFINE_TYPE_TAG(std::uint64_t, "uint64");

#undef DEFINE_TYPE_TAG

template<class Inspector, class T>
[[nodiscard]] auto process(Inspector& f, T& x) {
  using TT = std::remove_cvref_t<T>;
  static_assert(detail::IsInspectable<TT, Inspector>());
  if constexpr (detail::HasInspectOverload<TT, Inspector>::value) {
    return static_cast<Status>(inspect(f, x));
  } else if constexpr (detail::HasAccessSpecialization<TT>()) {
    return Access<T>::apply(f, x);
  } else if constexpr (detail::IsBuiltinType<TT>()) {
    return f.value(x);
  } else if constexpr (detail::IsTuple<TT>::value) {
    return f.tuple(x);
  } else if constexpr (detail::IsMapLike<TT>::value) {
    return f.map(x);
  } else if constexpr (detail::IsListLike<TT>::value) {
    return f.list(x);
  }
}

template<class Inspector, class T>
[[nodiscard]] auto process(Inspector& f, T const& x) {
  static_assert(!Inspector::isLoading);
  return process(f, const_cast<T&>(x));
}

template<class Inspector, class Value>
[[nodiscard]] auto saveField(Inspector& f, std::string_view name,
                             bool hasFallback, Value& val) {
  return detail::AccessType<Value>::saveField(f, name, hasFallback, val);
}

template<class Inspector, class Value, class Transformer>
[[nodiscard]] auto saveTransformedField(Inspector& f, std::string_view name,
                                        bool hasFallback, Value& val,
                                        Transformer& transformer) {
  return detail::AccessType<Value>::saveTransformedField(f, name, hasFallback,
                                                         val, transformer);
}

template<class Inspector, class Value>
[[nodiscard]] auto loadField(Inspector& f, std::string_view name,
                             bool isPresent, Value& val) {
  return detail::AccessType<Value>::loadField(f, name, isPresent, val);
}

template<class Inspector, class Value, class ApplyFallback>
[[nodiscard]] auto loadField(Inspector& f, std::string_view name,
                             bool isPresent, Value& val,
                             ApplyFallback&& applyFallback) {
  return detail::AccessType<Value>::loadField(
      f, name, isPresent, val, std::forward<ApplyFallback>(applyFallback));
}

template<class Inspector, class Value, class Transformer>
[[nodiscard]] auto loadTransformedField(Inspector& f, std::string_view name,
                                        bool isPresent, Value& val,
                                        Transformer& transformer) {
  return detail::AccessType<Value>::loadTransformedField(f, name, isPresent,
                                                         val, transformer);
}

template<class Inspector, class Value, class ApplyFallback, class Transformer>
[[nodiscard]] auto loadTransformedField(Inspector& f, std::string_view name,
                                        bool isPresent, Value& val,
                                        ApplyFallback&& applyFallback,
                                        Transformer& transformer) {
  return detail::AccessType<Value>::loadTransformedField(
      f, name, isPresent, val, std::forward<ApplyFallback>(applyFallback),
      transformer);
}

template<class Value>
struct AccessBase {
  template<class Inspector>
  [[nodiscard]] static auto saveField(Inspector& f, std::string_view name,
                                      bool hasFallback, Value& val) {
    f.builder().add(VPackValue(name));
    return f.apply(val);
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static auto saveTransformedField(Inspector& f,
                                                 std::string_view name,
                                                 bool hasFallback, Value& val,
                                                 Transformer& transformer) {
    typename Transformer::SerializedType v;
    return transformer.toSerialized(val, v)  //
           | [&]() { return inspection::saveField(f, name, hasFallback, v); };
  }

  template<class Inspector>
  [[nodiscard]] static Status loadField(Inspector& f, std::string_view name,
                                        bool isPresent, Value& val) {
    if (isPresent) {
      return f.apply(val);
    }
    return {"Missing required attribute '" + std::string(name) + "'"};
  }

  template<class Inspector, class ApplyFallback>
  [[nodiscard]] static Status loadField(Inspector& f, std::string_view name,
                                        bool isPresent, Value& val,
                                        ApplyFallback&& applyFallback) {
    if (isPresent) {
      return f.apply(val);
    }
    std::forward<ApplyFallback>(applyFallback)(val);
    return {};
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static auto loadTransformedField(Inspector& f,
                                                 std::string_view name,
                                                 bool isPresent, Value& val,
                                                 Transformer& transformer) {
    typename Transformer::SerializedType v;
    return inspection::loadField(f, name, isPresent, v)             //
           | [&]() { return transformer.fromSerialized(v, val); };  //
  }

  template<class Inspector, class ApplyFallback, class Transformer>
  [[nodiscard]] static Status loadTransformedField(
      Inspector& f, std::string_view name, bool isPresent, Value& val,
      ApplyFallback&& applyFallback, Transformer& transformer) {
    if (isPresent) {
      typename Transformer::SerializedType v;
      return f.apply(v)                                               //
             | [&]() { return transformer.fromSerialized(v, val); };  //
    }
    std::forward<ApplyFallback>(applyFallback)(val);
    return {};
  }
};

template<class T>
struct OptionalAccess {
  using Base = Access<T>;

  template<class Inspector>
  [[nodiscard]] static Status apply(Inspector& f, T& val) {
    if constexpr (Inspector::isLoading) {
      if (f.slice().isNull()) {
        val.reset();
        return {};
      } else {
        val = Base::make();
        return f.apply(*val);
      }
    } else {
      if (val) {
        return f.apply(*val);
      }
      f.builder().add(VPackValue(velocypack::ValueType::Null));
      return {};
    }
  }

  template<class Inspector>
  [[nodiscard]] static auto saveField(Inspector& f, std::string_view name,
                                      bool hasFallback, T& val)
      -> decltype(inspection::saveField(f, name, hasFallback, *val)) {
    if (val) {
      return inspection::saveField(f, name, hasFallback, *val);
    } else if (hasFallback) {
      // if we have a fallback we must explicitly serialize this field as null
      f.builder().add(VPackValue(name));
      f.builder().add(VPackValue(velocypack::ValueType::Null));
    }
    return {};
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static Status saveTransformedField(Inspector& f,
                                                   std::string_view name,
                                                   bool hasFallback, T& val,
                                                   Transformer& transformer) {
    if (val) {
      typename Transformer::SerializedType v;
      return transformer.toSerialized(*val, v) |
             [&]() { return inspection::saveField(f, name, hasFallback, v); };
    }
    return {};
  }

  template<class Inspector>
  [[nodiscard]] static Status loadField(Inspector& f, std::string_view name,
                                        bool isPresent, T& val) {
    return loadField(f, name, isPresent, val, [](auto& v) { v.reset(); });
  }

  template<class Inspector, class ApplyFallback>
  [[nodiscard]] static Status loadField(Inspector& f,
                                        [[maybe_unused]] std::string_view name,
                                        bool isPresent, T& val,
                                        ApplyFallback&& applyFallback) {
    if (isPresent) {
      return f.apply(val);
    }
    std::forward<ApplyFallback>(applyFallback)(val);
    return {};
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static Status loadTransformedField(Inspector& f,
                                                   std::string_view name,
                                                   bool isPresent, T& val,
                                                   Transformer& transformer) {
    return loadTransformedField(
        f, name, isPresent, val, [](auto& v) { v.reset(); }, transformer);
  }

  template<class Inspector, class ApplyFallback, class Transformer>
  [[nodiscard]] static Status loadTransformedField(
      Inspector& f, [[maybe_unused]] std::string_view name, bool isPresent,
      T& val, ApplyFallback&& applyFallback, Transformer& transformer) {
    if (isPresent) {
      std::optional<typename Transformer::SerializedType> v;
      auto load = [&]() -> Status {
        if (!v) {
          val.reset();
          return {};
        }
        val = Base::make();
        return transformer.fromSerialized(*v, *val);
      };
      return f.apply(v)  //
             | load;     //
    }
    std::forward<ApplyFallback>(applyFallback)(val);
    return {};
  }
};

template<class T>
struct Access<std::optional<T>> : OptionalAccess<std::optional<T>> {
  static auto make() { return T{}; }
};

template<class T, class Deleter>
struct Access<std::unique_ptr<T, Deleter>>
    : OptionalAccess<std::unique_ptr<T, Deleter>> {
  static auto make() { return std::make_unique<T>(); }
};

template<class T>
struct Access<std::shared_ptr<T>> : OptionalAccess<std::shared_ptr<T>> {
  static auto make() { return std::make_shared<T>(); }
};

template<>
struct Access<std::monostate> : AccessBase<std::monostate> {
  template<class Inspector>
  static auto apply(Inspector& f, std::monostate&) {
    if constexpr (Inspector::isLoading) {
      if (!f.slice().isEmptyObject()) {
        return Status{"Expected empty object"};
      }
      return Status{};
    } else {
      f.builder().add(VPackSlice::emptyObjectSlice());
      return Status::Success{};
    }
  }
};

template<class... Ts>
struct Access<std::variant<Ts...>> : AccessBase<std::variant<Ts...>> {
  template<class Inspector>
  static auto apply(Inspector& f, std::variant<Ts...>& v) {
    if constexpr (Inspector::isLoading) {
      return f.beginObject()                  //
             | [&]() { return parse(f, v); }  //
             | [&]() { return f.endObject(); };
    } else {
      return f.beginObject()  //
             |
             [&]() {
               return std::visit(
                   [&]<typename T>(T const& arg) {
                     f.builder().add("tag", VPackValue(TypeTag<T>::name()));
                     return saveField(f, "value", false, arg);
                   },
                   v);
             }  //
             | [&]() { return f.endObject(); };
    }
  }

 private:
  template<class Inspector>
  static Status parse(Inspector& f, std::variant<Ts...>& result) {
    auto tag = f.slice()["tag"];
    if (!tag.isString()) {
      if (tag.isNone()) {
        return {"Variant tag is missing"};
      } else {
        return {"Variant tag must be a string"};
      }
    }
    auto value = f.slice()["value"];
    if (value.isNone()) {
      return {"Variant value is missing"};
    }
    return parseType<Ts...>(f, tag.stringView(), value, result);
  }

  template<class Head, class... Tail, class Inspector>
  static Status parseType(Inspector& f, std::string_view tag,
                          velocypack::Slice value,
                          std::variant<Ts...>& result) {
    if (TypeTag<Head>::name() == tag) {
      Inspector inspector(value, f.options());
      Head v;
      auto res = inspector.apply(v);
      if (res.ok()) {
        result = v;
        return res;
      } else {
        return {std::move(res), "value", Status::AttributeTag{}};
      }
    } else {
      if constexpr (sizeof...(Tail) == 0) {
        return {"Found invalid type tag: " + std::string(tag)};
      } else {
        return parseType<Tail...>(f, tag, value, result);
      }
    }
  }
};

}  // namespace arangodb::inspection

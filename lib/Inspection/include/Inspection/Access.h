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
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

namespace arangodb::inspection {

template<class T>
struct Access;

template<class T>
struct AccessBase;

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
  } else if constexpr (detail::IsListLike<TT>::value or
                       detail::IsSetLike<TT>::value) {
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
    return f.beginField(name)                //
           | [&]() { return f.apply(val); }  //
           | [&]() { return f.endField(); };
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
    typename Transformer::SerializedType v{};
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
      if (f.isNull()) {
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
      return f.apply(Null{});
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
      return f.beginField(name)                   //
             | [&]() { return f.apply(Null{}); }  //
             | [&]() { return f.endField(); };
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
      return f.beginObject()                      //
             | [&]() { return f.applyFields(); }  //
             | [&]() { return f.endObject(); };
    } else {
      return f.beginObject()  //
             | [&]() { return f.endObject(); };
    }
  }
};

template<>
struct Access<VPackBuilder> : AccessBase<VPackBuilder> {
  template<class Inspector>
  static auto apply(Inspector& f, VPackBuilder& x) {
    if constexpr (Inspector::isLoading) {
      x.clear();
      return f.value(x);
    } else {
      if (!x.isClosed()) {
        return Status{"Expected closed VPackBuilder"};
      }
      return f.apply(x.slice());
    }
  }
};

template<typename T>
struct Access<std::reference_wrapper<T>>
    : AccessBase<std::reference_wrapper<T>> {
  template<class Inspector>
  static auto apply(Inspector& f, std::reference_wrapper<T>& x) {
    static_assert(!Inspector::isLoading,
                  "a reference_wrapper cannot be deserialized because it "
                  "cannot be default constructed (default construction is "
                  "required for the deserialization result type)");
    return f.apply(x.get());
  }
};

template<class T, class StorageT>
struct StorageTransformerAccess {
  static_assert(std::is_same_v<T, typename StorageT::MemoryType>);

  template<class Inspector>
  static auto apply(Inspector& f, T& x) {
    if constexpr (Inspector::isLoading) {
      auto v = StorageT{};
      auto res = f.apply(v);
      if (res.ok()) {
        x = T(v);
      }
      return res;
    } else {
      auto v = StorageT(x);
      return f.apply(v);
    }
  }
};

/**
 * @brief This class behaves like an optional, but it will not serialize
 *        to `null` and `field: null` will fail to deserialize.
 *        This class cannot be embedded into a container (e.g. a vector)
 *        as the null-value cannot be serialized there.
 * @tparam T The actual data type.
 */

template<typename T>
struct NonNullOptional : std::optional<T> {
  using std::optional<T>::optional;

  bool operator==(NonNullOptional<T> const& other) const noexcept = default;

  template<typename U>
  bool operator==(U const& other) const noexcept {
    return this->has_value() && this->value() == other;
  }
};

template<class T>
struct Access<arangodb::inspection::NonNullOptional<T>> {
  template<class Inspector>
  [[nodiscard]] static Status apply(Inspector& f, NonNullOptional<T>& val) {
    static_assert(Inspector::isLoading && !Inspector::isLoading,
                  "Apply cannot be called, this type has to be a field and "
                  "cannot be embedded into a container.");
    // Added the return only to make some compilers happy.
    // The above assert is ALWAYS true => The method can never be
    // executed, or let alone end in this return statement.
    return {"internal-error: static assertion violated."};
  }

  template<class Inspector>
  [[nodiscard]] static auto saveField(Inspector& f, std::string_view name,
                                      bool hasFallback, NonNullOptional<T>& val)
      -> decltype(inspection::saveField(f, name, hasFallback, val.value())) {
    if (val) {
      return inspection::saveField(f, name, hasFallback, val.value());
    }
    return {};
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static Status saveTransformedField(Inspector& f,
                                                   std::string_view name,
                                                   bool hasFallback,
                                                   NonNullOptional<T>& val,
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
                                        bool isPresent,
                                        NonNullOptional<T>& val) {
    return loadField(f, name, isPresent, val, [](auto& v) { v.reset(); });
  }

  template<class Inspector, class ApplyFallback>
  [[nodiscard]] static Status loadField(Inspector& f,
                                        [[maybe_unused]] std::string_view name,
                                        bool isPresent, NonNullOptional<T>& val,
                                        ApplyFallback&& applyFallback) {
    if (isPresent) {
      T t{};
      auto res = f.apply(t);
      if (res.ok()) {
        val = {std::move(t)};
      }
      return res;
    }
    std::forward<ApplyFallback>(applyFallback)(val);
    return {};
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static Status loadTransformedField(Inspector& f,
                                                   std::string_view name,
                                                   bool isPresent,
                                                   NonNullOptional<T>& val,
                                                   Transformer& transformer) {
    return loadTransformedField(
        f, name, isPresent, val, [](auto& v) { v.reset(); }, transformer);
  }

  template<class Inspector, class ApplyFallback, class Transformer>
  [[nodiscard]] static Status loadTransformedField(
      Inspector& f, [[maybe_unused]] std::string_view name, bool isPresent,
      NonNullOptional<T>& val, ApplyFallback&& applyFallback,
      Transformer& transformer) {
    if (isPresent) {
      typename Transformer::SerializedType v{};
      auto load = [&]() -> Status {
        T t{};
        auto res = transformer.fromSerialized(v, t);
        if (res.ok()) {
          val = {std::move(t)};
        }
        return res;
      };
      return f.apply(v)  //
             | load;     //
    }
    std::forward<ApplyFallback>(applyFallback)(val);
    return {};
  }
};

}  // namespace arangodb::inspection

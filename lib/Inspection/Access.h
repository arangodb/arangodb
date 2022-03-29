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

#include <velocypack/Value.h>

#include "Inspection/detail/traits.h"

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
    return static_cast<Result>(inspect(f, x));
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
[[nodiscard]] Result process(Inspector& f, T const& x) {
  static_assert(!Inspector::isLoading);
  return process(f, const_cast<T&>(x));
}

template<class Inspector, class Value>
[[nodiscard]] auto saveField(Inspector& f, std::string_view name, Value& val) {
  return detail::AccessType<Value>::saveField(f, name, val);
}

template<class Inspector, class Value, class Transformer>
[[nodiscard]] auto saveTransformedField(Inspector& f, std::string_view name,
                                        Value& val, Transformer& transformer) {
  return detail::AccessType<Value>::saveTransformedField(f, name, val,
                                                         transformer);
}

template<class Inspector, class Value>
[[nodiscard]] Result loadField(Inspector& f, std::string_view name,
                               Value& val) {
  return detail::AccessType<Value>::loadField(f, name, val);
}

template<class Inspector, class Value, class ApplyFallback>
[[nodiscard]] Result loadField(Inspector& f, std::string_view name, Value& val,
                               ApplyFallback&& applyFallback) {
  return detail::AccessType<Value>::loadField(
      f, name, val, std::forward<ApplyFallback>(applyFallback));
}

template<class Inspector, class Value, class Transformer>
[[nodiscard]] Result loadTransformedField(Inspector& f, std::string_view name,
                                          Value& val,
                                          Transformer& transformer) {
  return detail::AccessType<Value>::loadTransformedField(f, name, val,
                                                         transformer);
}

template<class Inspector, class Value, class ApplyFallback, class Transformer>
[[nodiscard]] Result loadTransformedField(Inspector& f, std::string_view name,
                                          Value& val,
                                          ApplyFallback&& applyFallback,
                                          Transformer& transformer) {
  return detail::AccessType<Value>::loadTransformedField(
      f, name, val, std::forward<ApplyFallback>(applyFallback), transformer);
}

template<class Value>
struct AccessBase {
  template<class Inspector>
  [[nodiscard]] static auto saveField(Inspector& f, std::string_view name,
                                      Value& val) {
    f.builder().add(VPackValue(name));
    return f.apply(val);
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static Result saveTransformedField(Inspector& f,
                                                   std::string_view name,
                                                   Value& val,
                                                   Transformer& transformer) {
    typename Transformer::SerializedType v;
    return transformer.toSerialized(val, v)                        //
           | [&]() { return inspection::saveField(f, name, v); };  //
  }

  template<class Inspector>
  [[nodiscard]] static Result loadField(Inspector& f, std::string_view name,
                                        Value& val) {
    auto s = f.slice();
    if (s.isNone()) {
      return {"Missing required attribute '" + std::string(name) + "'"};
    }
    return f.apply(val);
  }

  template<class Inspector, class ApplyFallback>
  [[nodiscard]] static Result loadField(Inspector& f, std::string_view name,
                                        Value& val,
                                        ApplyFallback&& applyFallback) {
    auto s = f.slice();
    if (s.isNone()) {
      std::forward<ApplyFallback>(applyFallback)(val);
      return {};
    }
    return f.apply(val);
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static Result loadTransformedField(Inspector& f,
                                                   std::string_view name,
                                                   Value& val,
                                                   Transformer& transformer) {
    typename Transformer::SerializedType v;
    return inspection::loadField(f, name, v)                        //
           | [&]() { return transformer.fromSerialized(v, val); };  //
  }

  template<class Inspector, class ApplyFallback, class Transformer>
  [[nodiscard]] static Result loadTransformedField(
      Inspector& f, std::string_view name, Value& val,
      ApplyFallback&& applyFallback, Transformer& transformer) {
    auto s = f.slice();
    if (s.isNone()) {
      std::forward<ApplyFallback>(applyFallback)(val);
      return {};
    }
    typename Transformer::SerializedType v;
    return f.apply(v)                                               //
           | [&]() { return transformer.fromSerialized(v, val); };  //
  }
};

template<class T>
struct Access<std::optional<T>> {
  template<class Inspector>
  [[nodiscard]] static Result apply(Inspector& f, std::optional<T>& val) {
    if constexpr (Inspector::isLoading) {
      if (f.slice().isNone() || f.slice().isNull()) {
        val.reset();
        return {};
      } else {
        T v;
        auto assign = [&]() -> Result::Success {
          val = std::move(v);
          return {};
        };
        return f.apply(v)  //
               | assign;   //
      }
    } else {
      if (val.has_value()) {
        return f.apply(val.value());
      }
      f.builder().add(VPackValue(velocypack::ValueType::Null));
      return {};
    }
  }

  template<class Inspector>
  [[nodiscard]] static auto saveField(Inspector& f, std::string_view name,
                                      std::optional<T>& val)
      -> decltype(inspection::saveField(f, name, val.value())) {
    if (!val.has_value()) {
      return {};
    }
    return inspection::saveField(f, name, val.value());
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static Result saveTransformedField(Inspector& f,
                                                   std::string_view name,
                                                   std::optional<T>& val,
                                                   Transformer& transformer) {
    if (!val.has_value()) {
      return {};
    }
    typename Transformer::SerializedType v;
    return transformer.toSerialized(*val, v) |
           [&]() { return inspection::saveField(f, name, v); };
  }

  template<class Inspector>
  [[nodiscard]] static Result loadField(Inspector& f,
                                        [[maybe_unused]] std::string_view name,
                                        std::optional<T>& val) {
    return f.apply(val);
  }

  template<class Inspector, class ApplyFallback>
  [[nodiscard]] static Result loadField(Inspector& f,
                                        [[maybe_unused]] std::string_view name,
                                        std::optional<T>& val,
                                        ApplyFallback&& applyFallback) {
    auto s = f.slice();
    if (s.isNone()) {
      std::forward<ApplyFallback>(applyFallback)(val);
      return {};
    }
    return f.apply(val);
  }

  template<class Inspector, class Transformer>
  [[nodiscard]] static Result loadTransformedField(
      Inspector& f, [[maybe_unused]] std::string_view name,
      std::optional<T>& val, Transformer& transformer) {
    std::optional<typename Transformer::SerializedType> v;
    auto load = [&]() -> Result {
      if (!v.has_value()) {
        val.reset();
        return {};
      }
      T vv;
      auto res = transformer.fromSerialized(*v, vv);
      val = vv;
      return res;
    };
    return f.apply(v)  //
           | load;     //
  }

  template<class Inspector, class ApplyFallback, class Transformer>
  [[nodiscard]] static Result loadTransformedField(
      Inspector& f, std::string_view name, std::optional<T>& val,
      ApplyFallback&& applyFallback, Transformer& transformer) {
    auto s = f.slice();
    if (s.isNone()) {
      std::forward<ApplyFallback>(applyFallback)(val);
      return {};
    }
    return loadTransformedField(f, name, val, transformer);
  }
};

template<class Derived, class T>
struct PointerAccess {
  template<class Inspector>
  [[nodiscard]] static Result apply(Inspector& f, T& val) {
    if constexpr (Inspector::isLoading) {
      if (f.slice().isNone() || f.slice().isNull()) {
        val.reset();
        return {};
      }
      val = Derived::make();  // TODO - reuse existing object?
      return f.apply(*val);
    } else {
      if (val != nullptr) {
        return f.apply(*val);
      }
      f.builder().add(VPackValue(velocypack::ValueType::Null));
      return {};
    }
  }

  template<class Inspector>
  [[nodiscard]] static auto saveField(Inspector& f, std::string_view name,
                                      T& val)
      -> decltype(inspection::saveField(f, name, *val)) {
    if (val == nullptr) {
      return {};
    }
    return inspection::saveField(f, name, *val);
  }

  template<class Inspector>
  [[nodiscard]] static Result loadField(Inspector& f,
                                        [[maybe_unused]] std::string_view name,
                                        T& val) {
    auto s = f.slice();
    if (s.isNone() || s.isNull()) {
      val.reset();
      return {};
    }
    val = Derived::make();  // TODO - reuse existing object?
    return f.apply(val);
  }

  template<class Inspector, class ApplyFallback>
  [[nodiscard]] static Result loadField(Inspector& f,
                                        [[maybe_unused]] std::string_view name,
                                        T& val, ApplyFallback&& applyFallback) {
    auto s = f.slice();
    if (s.isNone()) {
      std::forward<ApplyFallback>(applyFallback)(val);
      return {};
    } else if (s.isNull()) {
      val.reset();
      return {};
    }
    val = Derived::make();  // TODO - reuse existing object?
    return f.apply(val);
  }
};

template<class T, class Deleter>
struct Access<std::unique_ptr<T, Deleter>>
    : PointerAccess<Access<std::unique_ptr<T, Deleter>>,
                    std::unique_ptr<T, Deleter>> {
  static auto make() { return std::make_unique<T>(); }
};

template<class T>
struct Access<std::shared_ptr<T>>
    : PointerAccess<Access<std::shared_ptr<T>>, std::shared_ptr<T>> {
  static auto make() { return std::make_shared<T>(); }
};

}  // namespace arangodb::inspection

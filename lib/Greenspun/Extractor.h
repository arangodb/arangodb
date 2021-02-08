////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GREENSPUN_EXTRACTOR_H
#define ARANGODB_GREENSPUN_EXTRACTOR_H 1

#include "EvalResult.h"
#include "Interpreter.h"
#include "Basics/VelocyPackHelper.h"


namespace arangodb::greenspun {

template<typename T, typename C = void>
struct extractor {
  template<typename E>
  static inline constexpr auto always_false_v = std::false_type::value;
  static_assert(always_false_v<T>, "no extractor for that type available");
};

template<>
struct extractor<std::string> {
  EvalResultT<std::string> operator()(VPackSlice slice) {
    if (slice.isString()) {
      return {slice.copyString()};
    }
    return EvalError("expected string, found: " + slice.toJson());
  }
};

template<>
struct extractor<std::string_view> {
  EvalResultT<std::string_view> operator()(VPackSlice slice) {
    if (slice.isString()) {
      return {slice.stringView()};
    }
    return EvalError("expected string, found: " + slice.toJson());
  }
};

template<typename T>
struct extractor<T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<bool, T>>> {
  EvalResultT<T> operator()(VPackSlice slice) {
    if (slice.isNumber<T>()) {
      return {slice.getNumber<T>()};
    }
    return EvalError("expected number, found: " + slice.toJson());
  }
};

template<>
struct extractor<bool> {
  EvalResultT<bool> operator()(VPackSlice slice) {
    return {ValueConsideredTrue(slice)};
  }
};

template<>
struct extractor<VPackArrayIterator> {
  EvalResultT<VPackArrayIterator> operator()(VPackSlice slice) {
    if (slice.isArray()) {
      return {VPackArrayIterator(slice)};
    }
    return EvalError("expected list, found: " + slice.toJson());
  }
};

template<>
struct extractor<VPackSlice> {
  EvalResultT<VPackSlice> operator()(VPackSlice slice) {
    return {slice};
  }
};


template<typename T>
auto extractValue(VPackSlice value) -> EvalResultT<T> {
  static_assert(std::is_invocable_r_v<EvalResultT<T>, extractor<T>, VPackSlice>, "bad signature for extractor");
  return extractor<T>{}(value);
}

template<typename T, typename... Ts>
auto extractFromArray(VPackArrayIterator iter) -> EvalResultT<std::tuple<T, Ts...>> {
  if constexpr (sizeof...(Ts) == 0) {
    return extractValue<T>(*iter)
        .map([](auto&& v) {
          return std::make_tuple(std::forward<decltype(v)>(v));
        })
        .mapError([&](EvalError& err) {
          err.wrapMessage("at parameter " + std::to_string(iter.index() + 1));
        });
  } else {
    auto result = extractValue<T>(*iter);
    if (!result) {
      return result.error().wrapMessage("at parameter " + std::to_string(iter.index() + 1));
    }

    return extractFromArray<Ts...>(++iter).map([&](auto&& t) {
      return std::tuple_cat(std::make_tuple(std::move(result).value()), t);
    });
  }
}

template <typename... Ts>
auto extract(VPackSlice values) -> EvalResultT<std::tuple<Ts...>> {
  if (values.isArray()) {
    if (values.length() != sizeof...(Ts)) {
      return EvalError("found " + std::to_string(values.length()) +
                       " argument(s), expected " + std::to_string(sizeof...(Ts)));
    }

    if constexpr (sizeof...(Ts) > 0) {
      return extractFromArray<Ts...>(VPackArrayIterator{values});
    } else {
      return {};
    }
  }

  return EvalError("expected parameter array, found: " + values.toJson());
}

}

#endif  // ARANGODB3_EXTRACTOR_H

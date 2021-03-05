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

#ifndef ARANGODB_GREENSPUN_EVALRESULT_H
#define ARANGODB_GREENSPUN_EVALRESULT_H 1

#include <vector>
#include <string>
#include <variant>

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace greenspun {

struct EvalError {
  struct CallFrame {
    std::string function;
    std::vector<std::string> parameter;
  };

  struct SpecialFormFrame {
    std::string specialForm;
  };

  struct ParamFrame {
    std::string function;
    std::size_t offset;
  };

  struct WrapFrame {
    std::string message;
  };

  using Frame = std::variant<ParamFrame, CallFrame, WrapFrame, SpecialFormFrame>;

  explicit EvalError(std::string message) : message(std::move(message)) {}
  EvalError(EvalError const&) = default;
  EvalError(EvalError&&) = default;
  EvalError& operator=(EvalError const&) = default;
  EvalError& operator=(EvalError&&) = default;

  EvalError& wrapParameter(std::string function, std::size_t off) {
    frames.emplace_back(ParamFrame{std::move(function), off});
    return *this;
  }

  EvalError& wrapMessage(std::string wrap) {
    frames.emplace_back(WrapFrame{std::move(wrap)});
    return *this;
  }

  EvalError& wrapCall(std::string function, VPackSlice parameter) {
    std::vector<std::string> parameterVec;
    parameterVec.reserve(parameter.length());
    for (auto&& p : VPackArrayIterator(parameter)) {
      parameterVec.push_back(p.toJson());
    }
    frames.emplace_back(CallFrame{std::move(function), std::move(parameterVec)});
    return *this;
  }

  EvalError& wrapSpecialForm(std::string function) {
    frames.emplace_back(SpecialFormFrame{std::move(function)});
    return *this;
  }

  std::string toString() const;

  std::vector<Frame> frames;
  std::string message;
};

template<typename T>
struct EvalResultT {
  bool ok() const { return hasValue(); }
  bool fail() const { return !ok(); }

  bool hasError() const { return _value.index() == 1; }
  bool hasValue() const { return _value.index() == 0; }

  operator bool() const { return ok(); }

  EvalResultT() = default;
  EvalResultT(EvalError error) : _value(std::in_place_index<1>, std::move(error)) {}
  EvalResultT(EvalResultT const&) = default;
  EvalResultT(EvalResultT&&) noexcept = default;
  EvalResultT& operator=(EvalResultT const&) = default;
  EvalResultT& operator=(EvalResultT&&) noexcept = default;

  EvalResultT(T s) : _value(std::in_place_index<0>, std::move(s)) {}


  template <typename F>
  EvalResultT mapError(F&& f) {
    if (hasError()) {
      std::forward<F>(f)(error());
    }
    return *this;
  }

  EvalResultT<std::monostate> asResult() && {
    return std::move(*this).map([](auto &&) -> std::monostate { return {}; });
  };

  template<typename F>
  auto map(F&& f) && -> EvalResultT<std::invoke_result_t<F, T&&>> {
    if (hasValue()) {
      return {f(value())};
    } else if(hasError()) {
      return error();
    } else {
      return EvalError("valueless by exception");
    }
  }

  T& value() & { return std::get<0>(_value); }
  T const& value() const& { return std::get<0>(_value); }
  T&& value() && { return std::get<0>(std::move(_value)); }
  EvalError& error() & { return std::get<1>(_value); }
  EvalError const& error() const& { return std::get<1>(_value); }
  EvalError&& error() && { return std::get<1>(std::move(_value)); }

  std::variant<T, EvalError> _value;
};

using EvalResult = EvalResultT<std::monostate>;

}  // namespace greenspun
}  // namespace arangodb

#endif

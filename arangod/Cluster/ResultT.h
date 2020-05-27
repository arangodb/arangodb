////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_RESULT_T_H
#define ARANGODB_BASICS_RESULT_T_H

#include <type_traits>

#include <optional>

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"

namespace arangodb {

// @brief Extension of Result which, on success, contains a value of type T.
//
// @details
// A `ResultT x` is expected to hold a value *if and only if* x.ok()!
// So this behaves more like a variant, even if it always contains a Result.
// This is to easily obtain compatibility with existing Result objects.
//
// A successful ResultT can be explicitly created via
//   ResultT::success(value);
// and an erroneous using
//   ResultT::error(TRI_SOME_ERROR)
// or
//   ResultT::error(TRI_ANOTHER_ERROR, "a custom message")
// . Never pass TRI_ERROR_NO_ERROR (i.e. 0) here! Use ::success() for that.
//
// Successful construction is also allowed via the constructor(s)
//  ResultT(T val)
// while error construction is allowed via the constructor(s)
//  ResultT(Result other)
// . Both of those allow implicit conversion, so if you have a function
// returning `ResultT<SomeType>` and a `SomeType value` you can use
//   return value;
// or, having a `Result result` with result.fail() being true,
//   return result;
// .
// Some implicit conversions are disabled when they could cause ambiguity.
template <typename T>
class ResultT {
 public:
  ResultT static success(T const& val) {
    return ResultT(val, TRI_ERROR_NO_ERROR);
  }

  ResultT static success(T&& val) {
    return ResultT(std::move(val), TRI_ERROR_NO_ERROR);
  }

  ResultT static error(int errorNumber) {
    return ResultT(std::nullopt, errorNumber);
  }

  ResultT static error(int errorNumber, std::string const& errorMessage) {
    return ResultT(std::nullopt, errorNumber, errorMessage);
  }

  ResultT static error(Result const& other) {
    TRI_ASSERT(other.fail());
    return ResultT(std::nullopt, other);
  }

  ResultT static error(Result&& other) {
    TRI_ASSERT(other.fail());
    return ResultT(std::nullopt, std::move(other));
  }

  // This is disabled if U is implicitly convertible to Result
  // (e.g., if U = int) to avoid ambiguous construction.
  // Use ::success() or ::error() instead in that case.
  template <typename U = T, typename = std::enable_if_t<!std::is_convertible<U, Result>::value>>
  // This is not explicit on purpose
  // NOLINTNEXTLINE(google-explicit-constructor)
  // cppcheck-suppress noExplicitConstructor
  /* implicit */ ResultT(Result const& other) : _result(other) {
    // .ok() is not allowed here, as _val should be expected to be initialized
    // iff .ok() is true.
    TRI_ASSERT(_result.fail());
  }

  // This is disabled if U is implicitly convertible to Result
  // (e.g., if U = int) to avoid ambiguous construction.
  // Use ::success() or ::error() instead in that case.
  template <typename U = T, typename = std::enable_if_t<!std::is_convertible<U, Result>::value>>
  // This is not explicit on purpose
  // NOLINTNEXTLINE(google-explicit-constructor)
  // cppcheck-suppress noExplicitConstructor
  /* implicit */ ResultT(Result&& other) : _result(std::move(other)) {
    // .ok() is not allowed here, as _val should be expected to be initialized
    // iff .ok() is true.
    TRI_ASSERT(_result.fail());
  }

  // This is disabled if U is implicitly convertible to Result
  // (e.g., if U = int) to avoid ambiguous construction.
  // Use ::success() or ::error() instead in that case.
  template <typename U = T, typename = std::enable_if_t<!std::is_convertible<U, Result>::value>>
  // This is not explicit on purpose
  // NOLINTNEXTLINE(google-explicit-constructor)
  // cppcheck-suppress noExplicitConstructor
  /* implicit */ ResultT(T&& val) : ResultT(std::move(val), TRI_ERROR_NO_ERROR) {}

  // This is disabled if U is implicitly convertible to Result
  // (e.g., if U = int) to avoid ambiguous construction.
  // Use ::success() or ::error() instead in that case.
  template <typename U = T, typename = std::enable_if_t<!std::is_convertible<U, Result>::value>>
  // This is not explicit on purpose
  // NOLINTNEXTLINE(google-explicit-constructor)
  // cppcheck-suppress noExplicitConstructor
  /* implicit */ ResultT(T const& val) : ResultT(val, TRI_ERROR_NO_ERROR) {}

  ResultT() = delete;

  ResultT& operator=(T const& val_) {
    _val = val_;
    return *this;
  }

  ResultT& operator=(T&& val_) {
    _val = std::move(val_);
    return *this;
  }

  // These would be very convenient, but also make it very easy to accidentally
  // use the value of an error-result. So don't add them.
  //
  //  operator T() const { return get(); }
  //  operator T &() { return get(); }

  T* operator->() { return &get(); }

  T const* operator->() const { return &get(); }

  T& operator*() & { return get(); }

  T const& operator*() const& { return get(); }

  T&& operator*() && { return get(); }

  T const&& operator*() const&& { return get(); }

  // Allow convenient check to allow for code like
  //   if (res) { /* use res.get() */ } else { /* handle error res.result() */ }
  // . Is disabled for bools to avoid accidental confusion of
  //   if (res)
  // with
  //   if (res.get())
  // .
  template <typename U = T, typename = std::enable_if_t<!std::is_same<U, bool>::value>>
  explicit operator bool() const {
    return ok();
  }

  T const& get() const { return _val.value(); }

  T& get() { return _val.value(); }

  ResultT map(ResultT<T> (*fun)(T const& val)) const {
    if (ok()) {
      return ResultT<T>::success(fun(get()));
    }

    return *this;
  }

  bool operator==(ResultT<T> const& other) const {
    if (this->ok() && other.ok()) {
      return this->get() == other.get();
    }
    if (this->fail() && other.fail()) {
      return this->errorNumber() == other.errorNumber() &&
             this->errorMessage() == other.errorMessage();
    }

    return false;
  }

  template <typename U>
  bool operator==(ResultT<U> const& other) const {
    if (this->fail() && other.fail()) {
      return this->errorNumber() == other.errorNumber() &&
             this->errorMessage() == other.errorMessage();
    }

    return false;
  }

  // forwarded methods
  bool ok() const { return _result.ok(); }
  bool fail() const { return _result.fail(); }
  bool is(int code) { return _result.is(code); }
  int errorNumber() const { return _result.errorNumber(); }
  std::string errorMessage() const { return _result.errorMessage(); }

  // access methods
  Result const& result() const& { return _result; }
  Result result() && { return std::move(_result); }

 protected:
  Result _result;
  std::optional<T> _val;

  ResultT(std::optional<T>&& val_, int errorNumber)
      : _result(errorNumber), _val(std::move(val_)) {}

  ResultT(std::optional<T>&& val_, int errorNumber, std::string const& errorMessage)
      : _result(errorNumber, errorMessage), _val(val_) {}

  ResultT(std::optional<T>&& val_, int errorNumber, std::string&& errorMessage)
      : _result(errorNumber, std::move(errorMessage)), _val(val_) {}

  ResultT(std::optional<T> const& val_, int errorNumber)
      : _result(errorNumber), _val(std::move(val_)) {}

  ResultT(std::optional<T> const& val_, int errorNumber, std::string const& errorMessage)
      : _result(errorNumber, errorMessage), _val(val_) {}

  ResultT(std::optional<T> const& val_, int errorNumber, std::string&& errorMessage)
      : _result(errorNumber, std::move(errorMessage)), _val(val_) {}

  ResultT(std::optional<T>&& val_, Result const& result)
      : _result(result), _val(std::move(val_)) {}

  ResultT(std::optional<T>&& val_, Result&& result)
      : _result(std::move(result)), _val(std::move(val_)) {}
};

}  // namespace arangodb

#endif  // ARANGODB_BASICS_RESULT_T_H

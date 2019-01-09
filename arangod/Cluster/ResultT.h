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

#include <boost/optional.hpp>

#include "Basics/Common.h"
#include "Basics/Result.h"

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
template <typename T>
class ResultT : public arangodb::Result {
 public:
  ResultT static success(T const& val) {
    return ResultT(val, TRI_ERROR_NO_ERROR);
  }

  ResultT static success(T&& val) {
    return ResultT(std::move(val), TRI_ERROR_NO_ERROR);
  }

  ResultT static error(int errorNumber) {
    return ResultT(boost::none, errorNumber);
  }

  ResultT static error(int errorNumber, std::string const& errorMessage) {
    return ResultT(boost::none, errorNumber, errorMessage);
  }

  // These are not explicit on purpose
  ResultT(Result const& other) : Result(other) {
    // .ok() is not allowed here, as _val should be expected to be initialized
    // iff .ok() is true.
    TRI_ASSERT(other.fail());
  }

  ResultT(Result&& other) : Result(std::move(other)) {
    // .ok() is not allowed here, as _val should be expected to be initialized
    // iff .ok() is true.
    TRI_ASSERT(other.fail());
  }

  // These are not explicit on purpose
  ResultT(T&& val) : ResultT(std::move(val), TRI_ERROR_NO_ERROR) {}

  ResultT(T const& val) : ResultT(val, TRI_ERROR_NO_ERROR) {}

  ResultT() = delete;

  ResultT& operator=(T const& val_) {
    _val = val_;
    return *this;
  }

  ResultT& operator=(T&& val_) {
    _val = std::move(val_);
    return *this;
  }

  Result copy_result() const { return *this; }

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

  explicit operator bool() const { return ok(); }

  T const& get() const { return _val.get(); }

  T& get() { return _val.get(); }

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

 protected:
  boost::optional<T> _val;

  ResultT(boost::optional<T>&& val_, int errorNumber)
      : Result(errorNumber), _val(std::move(val_)) {}

  ResultT(boost::optional<T>&& val_, int errorNumber, std::string const& errorMessage)
      : Result(errorNumber, errorMessage), _val(val_) {}

  ResultT(boost::optional<T> const& val_, int errorNumber)
      : Result(errorNumber), _val(std::move(val_)) {}

  ResultT(boost::optional<T> const& val_, int errorNumber, std::string const& errorMessage)
      : Result(errorNumber, errorMessage), _val(val_) {}
};

}  // namespace arangodb

#endif  // ARANGODB_BASICS_RESULT_T_H

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_RESULT_H
#define ARANGODB_BASICS_RESULT_H 1

#include "Basics/Common.h"
#include <type_traits>

namespace arangodb {

class Result {
 public:
  Result() : _errorNumber(TRI_ERROR_NO_ERROR) {}

  Result(int errorNumber)
      : _errorNumber(errorNumber){
    if (errorNumber != TRI_ERROR_NO_ERROR) {
      _errorMessage = TRI_errno_string(errorNumber);
    }
  }

  Result(int errorNumber, std::string const& errorMessage)
      : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

  Result(int errorNumber, std::string&& errorMessage)
      : _errorNumber(errorNumber), _errorMessage(std::move(errorMessage)) {}

  // copy
  Result(Result const& other)
      : _errorNumber(other._errorNumber),
        _errorMessage(other._errorMessage) {}

  Result& operator=(Result const& other) {
    _errorNumber = other._errorNumber;
    _errorMessage = other._errorMessage;
    return *this;
  }

  // move
  Result(Result&& other) noexcept
      : _errorNumber(other._errorNumber),
        _errorMessage(std::move(other._errorMessage)) {}

  Result& operator=(Result&& other) noexcept {
    _errorNumber = other._errorNumber;
    _errorMessage = std::move(other._errorMessage);
    return *this;
  }

 public:
  int errorNumber() const { return _errorNumber; }
  std::string errorMessage() const& { return _errorMessage; }
  std::string errorMessage() && { return std::move(_errorMessage); }

  bool ok()   const { return _errorNumber == TRI_ERROR_NO_ERROR; }
  bool fail() const { return !ok(); }

  bool is(int errorNumber) const { return _errorNumber == errorNumber; }
  bool isNot(int errorNumber) const { return !is(errorNumber); }

  Result& reset(int errorNumber = TRI_ERROR_NO_ERROR) {
    _errorNumber = errorNumber;

    if (errorNumber != TRI_ERROR_NO_ERROR) {
      _errorMessage = TRI_errno_string(errorNumber);
    } else {
      _errorMessage.clear();
    }
    return *this;
  }

  Result& reset(int errorNumber, std::string const& errorMessage) {
    _errorNumber = errorNumber;
    _errorMessage = errorMessage;
    return *this;
  }

  Result& reset(int errorNumber, std::string&& errorMessage) noexcept {
    _errorNumber = errorNumber;
    _errorMessage = std::move(errorMessage);
    return *this;
  }

  Result& reset(Result const& other) {
    _errorNumber = other._errorNumber;
    _errorMessage = other._errorMessage;
    return *this;
  }

  Result& reset(Result&& other) noexcept {
    _errorNumber = other._errorNumber;
    _errorMessage = std::move(other._errorMessage);
    return *this;
  }

 protected:
  int _errorNumber;
  std::string _errorMessage;

};


template <typename T>
struct TypedResult {
  //exception to the rule not _ here for mememers to allow easier access
  using ValueType = T;
  ValueType value;
private:
  bool _valid = false;
  Result _result;

public:
  TypedResult() = default;
  TypedResult(ValueType value, int error, std::string message)
    : _result(error, std::move(message))
    , value(std::move(value))
    , _valid(true)
    { }

  int errorNumber() const { return _result.errorMessage(); }
  std::string errorMessage() const& { return _result.errorMessage(); }
  std::string errorMessage() && { return std::move(std::move(_result).errorMessage()); }

  bool ok()   const { return _result.ok(); }
  bool fail() const { return !ok(); }

  bool is(int errorNumber) const { return _result.errorNumber() == errorNumber; }
  bool isNot(int errorNumber) const { return !is(errorNumber); }

  template <typename ...Args>
  TypedResult& reset(Args&&... args) {
    _result.reset(std::forward<Args>(args)...);
    return *this;
  }

  // should we mark that the result has been taken similar
  // to the value?
  Result getResult() &  { return _result; }
  Result getResult() && { return std::move(_result); }
  Result takeResult(){ return std::move(_result); }

  // check if we have valid result value;
  bool vaild(){ return _valid; }
  bool vaild(bool val){ _valid = val; return _valid; }

  T getValue() &  {
    //TRI_ASSERT(_valid);
    return value;
  }

  T getValue() && {
    //TRI_ASSERT(_valid);
    this->valid(false);
    return std::move(value);
  }

  T takeValue() && {
    //TRI_ASSERT(_valid);
    this->valid(false);
    return std::move(value);
  }

};

}
#endif

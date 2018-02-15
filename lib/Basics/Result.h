////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_RESULT_H
#define ARANGODB_BASICS_RESULT_H 1

#include "Basics/Common.h"
#include <type_traits>
#include <cstdint>


//#define RESULT_DEBUG true
#ifdef RESULT_DEBUG
  #include <iostream>
#endif

namespace arangodb {

class Result {
 public:
  Result() noexcept(std::is_nothrow_default_constructible<std::string>::value)
    : _errorNumber(TRI_ERROR_NO_ERROR) {}

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
struct ResultValue {
  //TODO: - fix / change formating to prevent Steemann going insane

  //exception to the rule: "value instead of _value" to allow easier access to members
  using ValueType = T;
  ValueType value;
private:
  bool _valid = false;
  Result _result;

public:
//// constructors

  ResultValue() = default;

  // handling lvalue references and pointers
  template < bool x = std::is_lvalue_reference<T>::value ||
                      std::is_pointer<T>::value
           , typename std::enable_if<x,int>::type = 0
           >
  ResultValue( ValueType value
             , Result const& res = {}
             )
    : value(value)
    , _valid(true)
    , _result(res)
    {
#ifdef RESULT_DEBUG
      std::cerr << "ctor: lvalue ref / pointer - 0 copy" << std::endl;
#endif
    }

  template < bool x = std::is_lvalue_reference<T>::value ||
                      std::is_pointer<T>::value
           , typename std::enable_if<x,int>::type = 0
           >
  ResultValue( ValueType value
             , Result&& res
             )
    : value(value)
    , _valid(true)
    , _result(std::move(res))
    {
#ifdef RESULT_DEBUG
      std::cerr << "ctor: lvalue ref / pointer - 0 copy" << std::endl;
#endif
    }
  // handling lvalue references and pointers - end


  // handling lvalues
  template < int x = !std::is_reference<T>::value &&
                     !std::is_pointer<T>::value
           , typename std::enable_if<x,int>::type = 0
           >
  ResultValue( ValueType const& value
             , Result const& res = {}
             )
    : value(value) //copy here
    , _valid(true)
    , _result(res)
    {
#ifdef RESULT_DEBUG
      std::cerr << "ctor: lvalue - 1 copy" << std::endl;
#endif
    }

  template < int x = !std::is_reference<T>::value &&
                     !std::is_pointer<T>::value
           , typename std::enable_if<x,int>::type = 0
           >
  ResultValue( ValueType const& value
             , Result&& res
             )
    : value(value) //copy here
    , _valid(true)
    , _result(std::move(res))
    {
#ifdef RESULT_DEBUG
      std::cerr << "ctor: lvalue - 1 copy" << std::endl;
#endif
    }
  // handling lvalues - end


  // handling rvalue / copy
  template < std::uint32_t x = !std::is_reference<T>::value &&
                               !std::is_pointer<T>::value &&
                                std::is_move_constructible<T>::value
           , typename std::enable_if<x,int>::type = 0
           >
  ResultValue( ValueType&& value
             , Result const& res = {}
             )
    : value(std::move(value))
    , _valid(true)
    , _result(res)
    {
#ifdef RESULT_DEBUG
      std::cerr << "ctor: rvalue (move ctor) - 0 copy" << std::endl;
#endif
    }

  template < std::uint32_t x = !std::is_reference<T>::value &&
                               !std::is_pointer<T>::value &&
                                std::is_move_constructible<T>::value
           , typename std::enable_if<x,int>::type = 0
           >
  ResultValue( ValueType&& value
             , Result && res
             )
    : value(std::move(value))
    , _valid(true)
    , _result(std::move(res))
    {
#ifdef RESULT_DEBUG
      std::cerr << "ctor: rvalue (move ctor) - 0 copy" << std::endl;
#endif
    }
  // handling rvalue / copy - end


  // handling rvalue / assign
  template < std::uint64_t x = !std::is_reference<T>::value &&
                               !std::is_pointer<T>::value &&
                               !std::is_move_constructible<T>::value &&
                                std::is_move_assignable<T>::value
           , typename std::enable_if<x,int>::type = 0
           >
  ResultValue( ValueType&& val
             , Result const& res = {}
             )
    : value()
    , _valid(true)
    , _result(res)
    {
      this->value = std::move(value);
#ifdef RESULT_DEBUG
      std::cerr << "ctor: rvalue (move assign) - 0 copy" << std::endl;
#endif
    }

  template < std::uint64_t x = !std::is_reference<T>::value &&
                               !std::is_pointer<T>::value &&
                               !std::is_move_constructible<T>::value &&
                                std::is_move_assignable<T>::value
           , typename std::enable_if<x,int>::type = 0
           >
  ResultValue( ValueType&& val
             , Result&& res
             )
    : value()
    , _valid(true)
    , _result(std::move(res))
    {
      this->value = std::move(value);
#ifdef RESULT_DEBUG
      std::cerr << "ctor: rvalue (move assign) - 0 copy" << std::endl;
#endif
    }
  // handling rvalue / assign - end


  // forward to result's functions
  int errorNumber() const { return _result.errorNumber(); }
  std::string errorMessage() const& { return _result.errorMessage(); }
  std::string errorMessage() && { return std::move(std::move(_result).errorMessage()); }

  bool ok()   const { return _result.ok(); }
  bool fail() const { return !ok(); }
  bool is(int errorNumber) const { return _result.errorNumber() == errorNumber; }
  bool isNot(int errorNumber) const { return !is(errorNumber); }

  // this function does not modify the value - it behaves exactly as it
  // does for the standalone result
  template <typename ...Args>
  ResultValue& reset(Args&&... args) {
    _result.reset(std::forward<Args>(args)...);
    return *this;
  }

  // some functions to retrieve the internal result
  Result  copyResult() const &  { return _result; }          // object is lvalue
  Result  copyResult() &&  { return std::move(_result); }    // object is rvalue
  Result  takeResult() { return std::move(_result); }        // all value types
  Result& getResult() const &  { return _result; }           // get only on lvalues

  // check if we have valid result value - this is not mandatory
  // it allows us to use values instead of pointers if an optional result is required
  bool vaild(){ return _valid; }
  bool vaild(bool val){ _valid = val; return _valid; }

};

}
#endif

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
class Result final {
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
  bool ok()   const { return _errorNumber == TRI_ERROR_NO_ERROR; }
  bool fail() const { return !ok(); }

  int errorNumber() const { return _errorNumber; }
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

  // the default implementation is const, but sub-classes might
  // really do more work to compute.

  std::string errorMessage() const& { return _errorMessage; }
  std::string errorMessage() && { return std::move(_errorMessage); }

 protected:
  int _errorNumber;
  std::string _errorMessage;
};

//a more complex result
class AnyResult {
    struct AnyBase {
      virtual ~AnyBase() = default;
      virtual const std::type_info& type() const noexcept = 0;
      virtual AnyBase* clone() const = 0;
    };

    template <typename T>
    struct Any : public AnyBase {
      T _value;
      Any(const T& value) : _value(value){}
      Any(const T&& value) : _value(std::move(value)){}
      Any& operator=(const Any&) = delete;

      virtual const std::type_info& type() const noexcept {
        return typeid(T);
      }

      virtual Any* clone() const {
        return new Any(_value);
      }
    };

    template <typename T>
    friend T* result_cast(AnyResult* res) noexcept;

    std::unique_ptr<AnyBase> _value;
    int _errorNumber;
    std::string _errorMessage;

 public:
    // constructors
    AnyResult(int number = TRI_ERROR_NO_ERROR) noexcept
      : _value(nullptr)
      , _errorNumber(number)
      {}

    AnyResult(AnyResult const& other, int number = TRI_ERROR_NO_ERROR, std::string message = "")
      : _value(other._value ? other._value->clone() : nullptr)
      , _errorNumber(number)
      , _errorMessage(message)
      {}


    template <typename T>
    AnyResult(T const& value) : _value(new Any<typename std::remove_cv<typename std::decay<T>::type
                                                                      >::type
                                              >(value)
                                      ){}

    template <typename T>
    AnyResult(T&& value
             ,typename std::enable_if<(!
                                       std::is_same<AnyResult&,T>::value ||
                                       std::is_const<T>::value
                                     )>::type = 0
             ) : _value(new Any<typename std::decay<T>::type>(std::move(value))) {}

    AnyResult(AnyResult&& other) noexcept : _value(std::move(other._value)){
      other._value.reset();
    }

    // assignment
    AnyResult& operator=(AnyResult& other) noexcept {
      AnyResult(std::move(other));
      return *this;
    }

    AnyResult& operator=(AnyResult&& other){
      AnyResult(std::move(other));
      return *this;
    }

    // other functions
    bool empty() const noexcept { return !_value; }
    void clear() noexcept { _value.reset(nullptr); } //clear the stored value
    std::type_info const& type() const noexcept {
      return _value ? _value->type()
                    : typeid(void);
    }

    bool ok()   const { return _errorNumber == TRI_ERROR_NO_ERROR; }
    bool fail() const { return !ok(); }

    int errorNumber() const { return _errorNumber; }
    bool is(int errorNumber) const { return _errorNumber == errorNumber; }
    bool isNot(int errorNumber) const { return !is(errorNumber); }

    std::string errorMessage() const& { return _errorMessage; }
    std::string errorMessage() && { return std::move(_errorMessage); }

    AnyResult& reset(int errorNumber = TRI_ERROR_NO_ERROR) {
      _errorNumber = errorNumber;

      if (errorNumber != TRI_ERROR_NO_ERROR) {
        _errorMessage = TRI_errno_string(errorNumber);
      } else {
        _errorMessage.clear();
      }
      return *this;
    }

    AnyResult& reset(int errorNumber, std::string const& errorMessage) {
      _errorNumber = errorNumber;
      _errorMessage = errorMessage;
      return *this;
    }

    AnyResult& reset(int errorNumber, std::string&& errorMessage) noexcept {
      _errorNumber = errorNumber;
      _errorMessage = std::move(errorMessage);
      return *this;
    }

    AnyResult& reset(AnyResult const& other) {
      _errorNumber = other._errorNumber;
      _errorMessage = other._errorMessage;
      return *this;
    }

    AnyResult& reset(AnyResult&& other) noexcept {
      _errorNumber = other._errorNumber;
      _errorMessage = std::move(other._errorMessage);
      return *this;
    }
};


// free functions
struct bad_result_cast : std::exception {
  virtual const char* what() const noexcept {
    return "bad_result_cast: failed conversion using result_cast";
  }
};

template <typename T>
T* result_cast(AnyResult* res) noexcept {
  return res && res->type() ==  typeid(T)
         ? &static_cast<AnyResult::Any<typename std::remove_cv<T>::type>*
                       > (res->_value)->value
         : nullptr;
}

template<typename T>
T const * result_cast(AnyResult const * res) noexcept {
  return result_cast<T>(const_cast<AnyResult*>(res));
}

template<typename T>
T result_cast(AnyResult& res) {
  using refremoved = typename ::std::remove_reference<T>::type;

  refremoved* rv = result_cast<refremoved>(&res);

  if(!rv){
    // we could add type id etc here
    throw (bad_result_cast());
  }

  // avoid temporary obj - *rv must not be temporary!!!!
  using restoredType =
    typename std::conditional<std::is_reference<T>::value
                             ,T
                             ,typename std::add_lvalue_reference<T>::type
                             >::type;

  return static_cast<restoredType>(*rv);
}

template <typename T>
T result_cast(AnyResult const& res){
  return result_cast<const typename std::remove_reference<T>::type &
                    >(const_cast<AnyResult&>(res));
}
}


#endif

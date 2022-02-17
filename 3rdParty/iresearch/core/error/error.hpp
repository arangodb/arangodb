////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ERROR_H
#define IRESEARCH_ERROR_H

#include <exception>

#include "utils/string.hpp"

namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @enum ErrorCode
//////////////////////////////////////////////////////////////////////////////
enum class ErrorCode : uint32_t {
  no_error = 0U,
  not_supported,
  io_error,
  eof_error,
  lock_obtain_failed,
  file_not_found,
  index_not_found,
  index_error,
  not_impl_error,
  illegal_argument,
  illegal_state,
  undefined_error
};

#define DECLARE_ERROR_CODE(class_name) \
  static const ErrorCode CODE = ErrorCode::class_name; \
  virtual ::iresearch::ErrorCode code() const noexcept override { return CODE; }

//////////////////////////////////////////////////////////////////////////////
/// @struct error_base
//////////////////////////////////////////////////////////////////////////////
struct error_base: std::exception {
  virtual ErrorCode code() const noexcept { return ErrorCode::undefined_error; }
  virtual const char* what() const noexcept override;
}; // error_base

//////////////////////////////////////////////////////////////////////////////
/// @class detailed_error_base
//////////////////////////////////////////////////////////////////////////////
class detailed_error_base : public error_base {
 public:
  detailed_error_base() = default;

  explicit detailed_error_base(const char* error)
    : error_(error) {
  }

  explicit detailed_error_base(std::string&& error) noexcept
    : error_(std::move(error)) {
  }

  virtual const char* what() const noexcept override {
    return error_.c_str();
  }

 private:
  std::string error_;
}; // detailed_error_base

//////////////////////////////////////////////////////////////////////////////
/// @struct not_supported
//////////////////////////////////////////////////////////////////////////////
struct not_supported : error_base {
  DECLARE_ERROR_CODE(not_supported);

  virtual const char* what() const noexcept override;
}; // not_supported

//////////////////////////////////////////////////////////////////////////////
/// @struct io_error
//////////////////////////////////////////////////////////////////////////////
struct io_error : detailed_error_base {
  DECLARE_ERROR_CODE(io_error);

  io_error() = default;

  template<typename T>
  explicit io_error(T&& error)
    : detailed_error_base(std::forward<T>(error)) {
  }
}; // io_error

//////////////////////////////////////////////////////////////////////////////
/// @struct lock_obtain_failed
//////////////////////////////////////////////////////////////////////////////
class lock_obtain_failed : public error_base {
 public:
  DECLARE_ERROR_CODE(lock_obtain_failed);

  explicit lock_obtain_failed(irs::string_ref filename = "");
  virtual const char* what() const noexcept override;

 private:
  std::string error_;
}; // lock_obtain_failed

//////////////////////////////////////////////////////////////////////////////
/// @class file_not_found
//////////////////////////////////////////////////////////////////////////////
class file_not_found: public error_base {
 public:
  DECLARE_ERROR_CODE(file_not_found);

  explicit file_not_found(irs::string_ref filename = "");
  virtual const char* what() const noexcept override;

 private:
  std::string error_;
}; // file_not_found

//////////////////////////////////////////////////////////////////////////////
/// @struct file_not_found
//////////////////////////////////////////////////////////////////////////////
struct index_not_found : error_base {
  DECLARE_ERROR_CODE(index_not_found);

  virtual const char* what() const noexcept override;
}; // index_not_found

//////////////////////////////////////////////////////////////////////////////
/// @struct index_error
//////////////////////////////////////////////////////////////////////////////
struct index_error : detailed_error_base {
  DECLARE_ERROR_CODE(index_error);

  template<typename T>
  explicit index_error(T&& error)
    : detailed_error_base(std::forward<T>(error)) {
  }
}; // index_error

//////////////////////////////////////////////////////////////////////////////
/// @struct not_impl_error
//////////////////////////////////////////////////////////////////////////////
struct not_impl_error : error_base {
  DECLARE_ERROR_CODE(not_impl_error);

  virtual const char* what() const noexcept override;
}; // not_impl_error

//////////////////////////////////////////////////////////////////////////////
/// @struct illegal_argument
//////////////////////////////////////////////////////////////////////////////
struct illegal_argument : detailed_error_base {
  DECLARE_ERROR_CODE(illegal_argument);

  template<typename T>
  explicit illegal_argument(T&& error)
    : detailed_error_base(std::forward<T>(error)) {
  }
}; // illegal_argument

//////////////////////////////////////////////////////////////////////////////
/// @struct illegal_state
//////////////////////////////////////////////////////////////////////////////
struct illegal_state : detailed_error_base {
  DECLARE_ERROR_CODE(illegal_state);

  template<typename T>
  explicit illegal_state(T&& error)
    : detailed_error_base(std::forward<T>(error)) {
  }
}; // illegal_state

}

#endif

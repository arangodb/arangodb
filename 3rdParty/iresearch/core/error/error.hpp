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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ERROR_H
#define IRESEARCH_ERROR_H

#include <exception>

#include "utils/string.hpp"

MSVC_ONLY(class IRESEARCH_API std::exception);

NS_ROOT

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

#define DECLARE_ERROR_CODE(class_name) static const ErrorCode CODE = ErrorCode::class_name

// ----------------------------------------------------------------------------
//                                                                   error_base
// ----------------------------------------------------------------------------
struct IRESEARCH_API error_base: std::exception {
  virtual ErrorCode code() const NOEXCEPT;
  virtual const char* what() const NOEXCEPT override;
};

// -----------------------------------------------------------------------------
//                                                           detailed_error_base
// -----------------------------------------------------------------------------
class IRESEARCH_API detailed_error_base: public error_base {
 public:
  explicit detailed_error_base(std::string&& error) NOEXCEPT
    : error_(std::move(error)) {
  }

  detailed_error_base(const detailed_error_base& other)
    : error_(other.error_) {
  }

  detailed_error_base(detailed_error_base&& other) NOEXCEPT
    : error_(std::move(other.error_)) {
  }

  detailed_error_base& operator=(const detailed_error_base& other) = delete;
  detailed_error_base& operator=(detailed_error_base&& other) NOEXCEPT = delete;

  virtual const char* what() const NOEXCEPT final { return error_.c_str(); }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string error_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

// ----------------------------------------------------------------------------
//                                                                not_supported
// ----------------------------------------------------------------------------
struct IRESEARCH_API not_supported: error_base {
  DECLARE_ERROR_CODE( not_supported );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
};

// ----------------------------------------------------------------------------
//                                                                     io_error
// ----------------------------------------------------------------------------
struct IRESEARCH_API io_error: error_base {
  DECLARE_ERROR_CODE( io_error );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
};

// ----------------------------------------------------------------------------
//                                                                    eof_error
// ----------------------------------------------------------------------------
struct IRESEARCH_API eof_error: io_error {
  DECLARE_ERROR_CODE( eof_error );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
};

// ----------------------------------------------------------------------------
//                                                            detailed_io_error
// ----------------------------------------------------------------------------
struct IRESEARCH_API detailed_io_error: public detailed_error_base {
 public:
  explicit detailed_io_error(std::string&& error)
    : detailed_error_base(std::move(error)) {
  }

  DECLARE_ERROR_CODE(io_error);
  virtual ErrorCode code() const NOEXCEPT override { return CODE; }
};

// ----------------------------------------------------------------------------
//                                                           lock_obtain_failed
// ----------------------------------------------------------------------------
class IRESEARCH_API lock_obtain_failed: public error_base {
 public:
  DECLARE_ERROR_CODE( lock_obtain_failed );
  explicit lock_obtain_failed(const irs::string_ref& filename = irs::string_ref::NIL);
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string error_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

// ----------------------------------------------------------------------------
//                                                               file_not_found
// ----------------------------------------------------------------------------
class IRESEARCH_API file_not_found: public error_base {
 public:
  DECLARE_ERROR_CODE( file_not_found );
  explicit file_not_found(const irs::string_ref& filename = irs::string_ref::NIL);
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string error_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

// ----------------------------------------------------------------------------
//                                                              index_not_found
// ----------------------------------------------------------------------------
struct IRESEARCH_API index_not_found: error_base {
  DECLARE_ERROR_CODE( index_not_found );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
};

// ----------------------------------------------------------------------------
//                                                                  index_error
// ----------------------------------------------------------------------------
struct IRESEARCH_API index_error: detailed_error_base {
  explicit index_error(std::string&& error)
    : detailed_error_base(std::move(error)) {
  }

  DECLARE_ERROR_CODE(index_error);
  virtual ErrorCode code() const NOEXCEPT override { return CODE; }
};

// ----------------------------------------------------------------------------
//                                                               not_impl_error
// ----------------------------------------------------------------------------
struct IRESEARCH_API not_impl_error: error_base {
  DECLARE_ERROR_CODE( not_impl_error );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
};

// ----------------------------------------------------------------------------
//                                                             illegal_argument
// ----------------------------------------------------------------------------
struct IRESEARCH_API illegal_argument: error_base {
  DECLARE_ERROR_CODE( illegal_argument );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
};

// ----------------------------------------------------------------------------
//                                                                illegal_state
// ----------------------------------------------------------------------------
struct IRESEARCH_API illegal_state: error_base {
  DECLARE_ERROR_CODE( illegal_state );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
};

NS_END

#endif

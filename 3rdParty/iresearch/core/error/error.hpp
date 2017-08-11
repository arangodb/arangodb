//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_ERROR_H
#define IRESEARCH_ERROR_H

#include "utils/string.hpp"

#include <exception>

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
struct IRESEARCH_API detailed_io_error: io_error {
  DECLARE_ERROR_CODE(io_error);
  explicit detailed_io_error(const irs::string_ref& error = irs::string_ref::nil);
  explicit detailed_io_error(std::string&& error);
  explicit detailed_io_error(const char* error);
  detailed_io_error& operator<<(const irs::string_ref& error);
  detailed_io_error& operator<<(std::string&& error);
  detailed_io_error& operator<<(const char* error);
  virtual iresearch::ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
 private:
  std::string error_;
};

// ----------------------------------------------------------------------------
//                                                           lock_obtain_failed
// ----------------------------------------------------------------------------
struct IRESEARCH_API lock_obtain_failed: error_base {
  DECLARE_ERROR_CODE( lock_obtain_failed );
  explicit lock_obtain_failed(const irs::string_ref& filename = irs::string_ref::nil);
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
 private:
  std::string error_;
};

// ----------------------------------------------------------------------------
//                                                               file_not_found
// ----------------------------------------------------------------------------
struct IRESEARCH_API file_not_found: error_base {
  DECLARE_ERROR_CODE( file_not_found );
  explicit file_not_found(const irs::string_ref& filename = irs::string_ref::nil);
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
 private:
  std::string error_;
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
struct IRESEARCH_API index_error: error_base {
  DECLARE_ERROR_CODE( index_error );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT override;
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
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
#include <boost/exception/exception.hpp>
#include <boost/exception/info.hpp>

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

/* -------------------------------------------------------------------
 * error
 * ------------------------------------------------------------------*/

//TODO: boost::excception has no public interface
struct IRESEARCH_API error_base : virtual std::exception,
                                  virtual ::boost::exception {
  virtual ErrorCode code() const NOEXCEPT;
  virtual const char* what() const NOEXCEPT;
};

/* -------------------------------------------------------------------
 * not_supported
 * ------------------------------------------------------------------*/

struct IRESEARCH_API not_supported : virtual error_base {
  DECLARE_ERROR_CODE( not_supported );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
};

/* -------------------------------------------------------------------
 * io_error
 * ------------------------------------------------------------------*/

struct IRESEARCH_API io_error : virtual error_base {
  DECLARE_ERROR_CODE( io_error );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
};

/* -------------------------------------------------------------------
 * eof_error
 * ------------------------------------------------------------------*/

struct IRESEARCH_API eof_error : virtual io_error {
  DECLARE_ERROR_CODE( eof_error );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
};

// ----------------------------------------------------------------------------
//                                                            detailed_io_error
// ----------------------------------------------------------------------------
struct detailed_io_error: virtual iresearch::io_error {
  explicit detailed_io_error(const std::string& error): error_(error) {}
  explicit detailed_io_error(std::string&& error): error_(std::move(error)) {}
  virtual iresearch::ErrorCode code() const NOEXCEPT override{ return CODE; }
  virtual const char* what() const NOEXCEPT{ return error_.c_str(); }
 private:
   std::string error_;
};

/* -------------------------------------------------------------------
 * lock_obtain_failed
 * ------------------------------------------------------------------*/

struct IRESEARCH_API lock_obtain_failed : virtual error_base {
  struct tag_lock_name;
  typedef ::boost::error_info<struct tag_lock_name, std::string> lock_name;
  
  DECLARE_ERROR_CODE( lock_obtain_failed );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
};

/* -------------------------------------------------------------------
* file_not_found
* ------------------------------------------------------------------*/

struct IRESEARCH_API file_not_found : virtual error_base {
  struct tag_file_name;
  typedef ::boost::error_info < struct tag_file_name, std::string> file_name;
  
  DECLARE_ERROR_CODE( file_not_found );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
 protected:
  struct tag_what_err;
  typedef ::boost::error_info <struct tag_file_name, std::string> what_err;
};

/* -------------------------------------------------------------------
* index_not_found
* ------------------------------------------------------------------*/

struct IRESEARCH_API index_not_found : virtual error_base {
  DECLARE_ERROR_CODE( index_not_found );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
};

/* -------------------------------------------------------------------
* index_error
* ------------------------------------------------------------------*/

struct IRESEARCH_API index_error : virtual error_base {
  DECLARE_ERROR_CODE( index_error );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
};

/* -------------------------------------------------------------------
 * not_impl_error
 * ------------------------------------------------------------------*/

struct IRESEARCH_API not_impl_error : virtual error_base {
  DECLARE_ERROR_CODE( not_impl_error );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
};

/* -------------------------------------------------------------------
* illegal_argument
* ------------------------------------------------------------------*/

struct IRESEARCH_API illegal_argument : virtual error_base {
  struct tag_arg_name;
  typedef ::boost::error_info < struct tag_arg_name, std::string> arg_name;
  
  DECLARE_ERROR_CODE( illegal_argument );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
};

/* -------------------------------------------------------------------
* illegal_state
* ------------------------------------------------------------------*/

struct IRESEARCH_API illegal_state : virtual error_base {
  struct tag_arg_name;
  typedef ::boost::error_info < struct tag_arg_name, std::string> arg_name;

  DECLARE_ERROR_CODE( illegal_state );
  virtual ErrorCode code() const NOEXCEPT override;
  virtual const char* what() const NOEXCEPT;
};

NS_END

#endif

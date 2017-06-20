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

#include <boost/exception/get_error_info.hpp>

#include "shared.hpp"
#include "error.hpp"

NS_ROOT

/* -------------------------------------------------------------------
 * error_base
 * ------------------------------------------------------------------*/

ErrorCode error_base::code() const NOEXCEPT {
  return ErrorCode::undefined_error;
}

const char* error_base::what() const NOEXCEPT {
  return "An unspecified error has occured.";
}

 /* -------------------------------------------------------------------
  * not_supported
  * ------------------------------------------------------------------*/

ErrorCode not_supported::code() const NOEXCEPT {
  return CODE;
}

const char* not_supported::what() const NOEXCEPT { 
  return "Operation not supported."; 
}

/* -------------------------------------------------------------------
 * io_error
 * ------------------------------------------------------------------*/

ErrorCode io_error::code() const NOEXCEPT {
  return CODE;
}

const char* io_error::what() const NOEXCEPT { 
  return "An unspecified I/O error has occured."; 
}

/* -------------------------------------------------------------------
 * eof_error
 * ------------------------------------------------------------------*/

ErrorCode eof_error::code() const NOEXCEPT {
  return CODE;
}

const char* eof_error::what() const NOEXCEPT { 
  return "Read past EOF."; 
}

/* -------------------------------------------------------------------
 * lock_obtain_failed
 * ------------------------------------------------------------------*/

ErrorCode lock_obtain_failed::code() const NOEXCEPT {
  return CODE;
}

const char* lock_obtain_failed::what() const NOEXCEPT {
  return "Lock obtain timed out."; 
}

/* -------------------------------------------------------------------
* file_not_found
* ------------------------------------------------------------------*/

ErrorCode file_not_found::code() const NOEXCEPT{
  return CODE;
}

const char* file_not_found::what() const NOEXCEPT { 
  auto err = boost::get_error_info<what_err>(*this);

  if (!err) {
    auto file = boost::get_error_info<file_name>(*this);

    std::stringstream ss;
    ss << "File not found";
    if (file) {
      ss << ": " << *file;
    } else {
      ss << ".";
    }

    err = boost::get_error_info<what_err>(*this << what_err(ss.str()));
  }

  return err->c_str();
}

/* -------------------------------------------------------------------
* index_not_found
* ------------------------------------------------------------------*/

ErrorCode index_not_found::code() const NOEXCEPT{
  return CODE;
}

const char* index_not_found::what() const NOEXCEPT {
  return "No segments* file found.";
}

/* -------------------------------------------------------------------
* index_error
* ------------------------------------------------------------------*/

ErrorCode index_error::code() const NOEXCEPT{
  return CODE;
}

const char* index_error::what() const NOEXCEPT { 
  return "Index error."; 
}

/* -------------------------------------------------------------------
 * not_impl_error
 * ------------------------------------------------------------------*/

 ErrorCode not_impl_error::code() const NOEXCEPT{
  return CODE;
}

const char* not_impl_error::what() const NOEXCEPT { 
  return "Not implemented."; 
}

/* -------------------------------------------------------------------
* illegal_argument
* ------------------------------------------------------------------*/

ErrorCode illegal_argument::code() const NOEXCEPT{
  return CODE;
}

const char* illegal_argument::what() const NOEXCEPT{ 
  return "Invalid argument."; 
}

/* -------------------------------------------------------------------
* illegal_argument
* ------------------------------------------------------------------*/

ErrorCode illegal_state::code() const NOEXCEPT{
  return CODE;
}

const char* illegal_state::what() const NOEXCEPT{
  return "Illegal state."; 
}

NS_END

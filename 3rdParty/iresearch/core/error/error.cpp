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

#include "shared.hpp"
#include "error.hpp"

NS_ROOT

// ----------------------------------------------------------------------------
//                                                                   error_base
// ----------------------------------------------------------------------------

ErrorCode error_base::code() const NOEXCEPT {
  return ErrorCode::undefined_error;
}

const char* error_base::what() const NOEXCEPT {
  return "An unspecified error has occured.";
}

// ----------------------------------------------------------------------------
//                                                                not_supported
// ----------------------------------------------------------------------------

ErrorCode not_supported::code() const NOEXCEPT {
  return CODE;
}

const char* not_supported::what() const NOEXCEPT { 
  return "Operation not supported."; 
}

// ----------------------------------------------------------------------------
//                                                                     io_error
// ----------------------------------------------------------------------------

ErrorCode io_error::code() const NOEXCEPT {
  return CODE;
}

const char* io_error::what() const NOEXCEPT { 
  return "An unspecified I/O error has occured."; 
}

// ----------------------------------------------------------------------------
//                                                                    eof_error
// ----------------------------------------------------------------------------

ErrorCode eof_error::code() const NOEXCEPT {
  return CODE;
}

const char* eof_error::what() const NOEXCEPT { 
  return "Read past EOF."; 
}

// ----------------------------------------------------------------------------
//                                                           lock_obtain_failed
// ----------------------------------------------------------------------------

lock_obtain_failed::lock_obtain_failed(
    const irs::string_ref& filename /*= irs::string_ref::NIL*/
) : error_("Lock obtain timed out") {
  if (filename.null()) {
    error_ += ".";
  } else {
    error_ += ": ";
    error_ + filename.c_str();
  }
}

ErrorCode lock_obtain_failed::code() const NOEXCEPT {
  return CODE;
}

const char* lock_obtain_failed::what() const NOEXCEPT {
  return error_.c_str();
}

// ----------------------------------------------------------------------------
//                                                               file_not_found
// ----------------------------------------------------------------------------

file_not_found::file_not_found(
    const irs::string_ref& filename /*= irs::string_ref::NIL*/
): error_("File not found") {
  if (filename.null()) {
    error_ += ".";
  } else {
    error_ += ": ";
    error_ + filename.c_str();
  }
}

ErrorCode file_not_found::code() const NOEXCEPT{
  return CODE;
}

const char* file_not_found::what() const NOEXCEPT { 
  return error_.c_str();
}

// ----------------------------------------------------------------------------
//                                                              index_not_found
// ----------------------------------------------------------------------------

ErrorCode index_not_found::code() const NOEXCEPT{
  return CODE;
}

const char* index_not_found::what() const NOEXCEPT {
  return "No segments* file found.";
}

// ----------------------------------------------------------------------------
//                                                               not_impl_error
// ----------------------------------------------------------------------------

 ErrorCode not_impl_error::code() const NOEXCEPT{
  return CODE;
}

const char* not_impl_error::what() const NOEXCEPT { 
  return "Not implemented."; 
}

// ----------------------------------------------------------------------------
//                                                             illegal_argument
// ----------------------------------------------------------------------------

ErrorCode illegal_argument::code() const NOEXCEPT{
  return CODE;
}

const char* illegal_argument::what() const NOEXCEPT{ 
  return "Invalid argument."; 
}

// ----------------------------------------------------------------------------
//                                                             illegal_argument
// ----------------------------------------------------------------------------

ErrorCode illegal_state::code() const NOEXCEPT{
  return CODE;
}

const char* illegal_state::what() const NOEXCEPT{
  return "Illegal state."; 
}

NS_END // NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
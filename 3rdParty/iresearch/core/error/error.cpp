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

const char* error_base::what() const NOEXCEPT {
  return "An unspecified error has occured.";
}

// ----------------------------------------------------------------------------
//                                                                not_supported
// ----------------------------------------------------------------------------

const char* not_supported::what() const NOEXCEPT {
  return "Operation not supported."; 
}

// ----------------------------------------------------------------------------
//                                                                    eof_error
// ----------------------------------------------------------------------------

const char* eof_error::what() const NOEXCEPT { 
  return "Read past EOF."; 
}

// ----------------------------------------------------------------------------
//                                                           lock_obtain_failed
// ----------------------------------------------------------------------------

lock_obtain_failed::lock_obtain_failed(
    const irs::string_ref& filename /*= "" */
) : error_("Lock obtain timed out") {
  if (filename.null()) {
    error_ += ".";
  } else {
    error_ += ": ";
    error_ + filename.c_str();
  }
}

const char* lock_obtain_failed::what() const NOEXCEPT {
  return error_.c_str();
}

// ----------------------------------------------------------------------------
//                                                               file_not_found
// ----------------------------------------------------------------------------

file_not_found::file_not_found(
    const irs::string_ref& filename /*= "" */
): error_("File not found") {
  if (filename.null()) {
    error_ += ".";
  } else {
    error_ += ": ";
    error_ + filename.c_str();
  }
}

const char* file_not_found::what() const NOEXCEPT { 
  return error_.c_str();
}

// ----------------------------------------------------------------------------
//                                                              index_not_found
// ----------------------------------------------------------------------------

const char* index_not_found::what() const NOEXCEPT {
  return "No segments* file found.";
}

// ----------------------------------------------------------------------------
//                                                               not_impl_error
// ----------------------------------------------------------------------------

const char* not_impl_error::what() const NOEXCEPT { 
  return "Not implemented."; 
}

// ----------------------------------------------------------------------------
//                                                             illegal_argument
// ----------------------------------------------------------------------------

const char* illegal_argument::what() const NOEXCEPT{ 
  return "Invalid argument."; 
}

// ----------------------------------------------------------------------------
//                                                             illegal_argument
// ----------------------------------------------------------------------------

const char* illegal_state::what() const NOEXCEPT{
  return "Illegal state."; 
}

NS_END // NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

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

#include "shared.hpp"
#include "error.hpp"

namespace iresearch {

// ----------------------------------------------------------------------------
//                                                                   error_base
// ----------------------------------------------------------------------------

const char* error_base::what() const noexcept {
  return "An unspecified error has occured.";
}

// ----------------------------------------------------------------------------
//                                                                not_supported
// ----------------------------------------------------------------------------

const char* not_supported::what() const noexcept {
  return "Operation not supported."; 
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
    error_ += filename.c_str();
  }
}

const char* lock_obtain_failed::what() const noexcept {
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
    error_.append(filename.c_str(), filename.size());
  }
}

const char* file_not_found::what() const noexcept { 
  return error_.c_str();
}

// ----------------------------------------------------------------------------
//                                                              index_not_found
// ----------------------------------------------------------------------------

const char* index_not_found::what() const noexcept {
  return "No segments* file found.";
}

// ----------------------------------------------------------------------------
//                                                               not_impl_error
// ----------------------------------------------------------------------------

const char* not_impl_error::what() const noexcept { 
  return "Not implemented."; 
}

// ----------------------------------------------------------------------------
//                                                             illegal_argument
// ----------------------------------------------------------------------------

const char* illegal_argument::what() const noexcept{ 
  return "Invalid argument."; 
}

// ----------------------------------------------------------------------------
//                                                             illegal_argument
// ----------------------------------------------------------------------------

const char* illegal_state::what() const noexcept{
  return "Illegal state."; 
}

} // namespace iresearch {

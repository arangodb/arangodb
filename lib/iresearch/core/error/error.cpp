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

#include "error.hpp"

#include "shared.hpp"

namespace irs {

const char* error_base::what() const noexcept {
  return "An unspecified error has occured.";
}

const char* not_supported::what() const noexcept {
  return "Operation not supported.";
}

lock_obtain_failed::lock_obtain_failed(std::string_view filename /*= "" */
                                       )
  : error_("Lock obtain timed out") {
  if (IsNull(filename)) {
    error_ += ".";
  } else {
    error_ += ": ";
    error_ += filename;
  }
}

const char* lock_obtain_failed::what() const noexcept { return error_.c_str(); }

file_not_found::file_not_found(std::string_view filename /*= "" */
                               )
  : error_("File not found") {
  if (IsNull(filename)) {
    error_ += ".";
  } else {
    error_ += ": ";
    error_.append(filename);
  }
}

const char* file_not_found::what() const noexcept { return error_.c_str(); }

const char* index_not_found::what() const noexcept {
  return "No segments* file found.";
}

const char* not_impl_error::what() const noexcept { return "Not implemented."; }

}  // namespace irs

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

#include "version_utils.hpp"

namespace iresearch {
namespace version_utils {

const string_ref build_date() {
  static const std::string value = __DATE__;

  return value;
}

const string_ref build_id() {
  static const std::string value = {
    #include "utils/build_identifier.csx"
  };

  return value;
}

const string_ref build_time() {
  static const std::string value = __TIME__;

  return value;
}

const string_ref build_version() {
  static const std::string value = {
    #include "utils/build_version.csx"
  };

  return value;
}

}
}

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

#include "directory_attributes.hpp"

#include "error/error.hpp"

namespace irs {

void index_file_refs::clear() {
  refs_.visit([](const auto&, size_t) { return true; }, true);

  if (!refs_.empty()) {
    throw illegal_state{"Cannot clear ref_counter due to live refs."};
  }
}

directory_attributes::directory_attributes(std::unique_ptr<irs::encryption> enc)
  : enc_{std::move(enc)}, refs_{std::make_unique<index_file_refs>()} {}

}  // namespace irs

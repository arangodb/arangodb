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

#include "field_meta.hpp"

#include "shared.hpp"
#include "analysis/token_attributes.hpp"

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                         field_meta implementation
// -----------------------------------------------------------------------------

/*static*/ const field_meta field_meta::EMPTY;

field_meta::field_meta(field_meta&& rhs) noexcept
  : features(std::move(rhs.features)),
    name(std::move(rhs.name)),
    index_features(rhs.index_features) {
  rhs.index_features = IndexFeatures::NONE;
}

field_meta::field_meta(
    const string_ref& name,
    IndexFeatures index_features)
  : name(name.c_str(), name.size()),
    index_features(index_features) {
}

field_meta& field_meta::operator=(field_meta&& rhs) noexcept {
  if (this != &rhs) {
    features = std::move(rhs.features);
    name = std::move(rhs.name);
    index_features = rhs.index_features;
    rhs.index_features = IndexFeatures::NONE;
  }

  return *this;
}

bool field_meta::operator==(const field_meta& rhs) const {
  return index_features == rhs.index_features &&
         name == rhs.name &&
         features == rhs.features;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        column_meta implementation
// -----------------------------------------------------------------------------

column_meta::column_meta(column_meta&& rhs) noexcept
  : name(std::move(rhs.name)), id(rhs.id) {
  rhs.id = field_limits::invalid();
}

column_meta::column_meta(const string_ref& name, field_id id)
  : name(name.c_str(), name.size()), id(id) {
}

column_meta& column_meta::operator=(column_meta&& rhs) noexcept {
  if (this != &rhs) {
    name = std::move(rhs.name);
    id = rhs.id;
    rhs.id = field_limits::invalid();
  }

  return *this;
}

bool column_meta::operator==(const column_meta& rhs) const {
  return name == rhs.name;
}

}

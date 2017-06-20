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

#include "shared.hpp"
#include "field_meta.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                         field_meta implementation
// -----------------------------------------------------------------------------

/*static*/ const field_meta field_meta::EMPTY;

field_meta::field_meta(field_meta&& rhs) NOEXCEPT
  : features(std::move(rhs.features)),
    name(std::move(rhs.name)),
    norm(rhs.norm) {
  rhs.norm = type_limits<type_t::field_id_t>::invalid();
}

field_meta::field_meta(
    const string_ref& name,
    const flags& features,
    field_id norm /* = type_limits<type_t::field_id_t>::invalid() */)
  : features(features),
    name(name.c_str(), name.size()),
    norm(norm) {
}

field_meta& field_meta::operator=(field_meta&& rhs) NOEXCEPT {
  if (this != &rhs) {
    features = std::move(rhs.features);
    name = std::move(rhs.name);
    norm = rhs.norm;
    rhs.norm = type_limits<type_t::field_id_t>::invalid();
  }

  return *this;
}

bool field_meta::operator==(const field_meta& rhs) const {
  return features == rhs.features && name == rhs.name;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        column_meta implementation
// -----------------------------------------------------------------------------

column_meta::column_meta(column_meta&& rhs) NOEXCEPT
  : name(std::move(rhs.name)), id(rhs.id) {
  rhs.id = type_limits<type_t::field_id_t>::invalid();
}

column_meta::column_meta(const string_ref& name, field_id id)
  : name(name.c_str(), name.size()), id(id) {
}

column_meta& column_meta::operator=(column_meta&& rhs) NOEXCEPT {
  if (this != &rhs) {
    name = std::move(rhs.name);
    id = rhs.id;
    rhs.id = type_limits<type_t::field_id_t>::invalid();
  }

  return *this;
}

bool column_meta::operator==(const column_meta& rhs) const {
  return name == rhs.name;
}

NS_END

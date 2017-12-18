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

#ifndef IRESEARCH_FIELD_META_H
#define IRESEARCH_FIELD_META_H

#include "utils/type_limits.hpp"
#include "utils/attributes.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @struct field_meta 
/// @brief represents field metadata
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API field_meta { 
 public:
  static const field_meta EMPTY;

  field_meta() = default;
  field_meta(const field_meta&) = default;
  field_meta(field_meta&& rhs) NOEXCEPT;
  field_meta(
    const string_ref& field, 
    const flags& features, 
    field_id norm = type_limits<type_t::field_id_t>::invalid()
  );

  field_meta& operator=(field_meta&& rhs) NOEXCEPT;
  field_meta& operator=(const field_meta&) = default;

  bool operator==(const field_meta& rhs) const;
  bool operator!=(const field_meta& rhs) const {
    return !(*this == rhs);
  }

  flags features;
  std::string name;
  field_id norm{ type_limits<type_t::field_id_t>::invalid() };
}; // field_meta

//////////////////////////////////////////////////////////////////////////////
/// @struct column_meta 
/// @brief represents column metadata
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API column_meta { 
 public:
  column_meta() = default;
  column_meta(const column_meta&) = default;
  column_meta(column_meta&& rhs) NOEXCEPT;
  column_meta(const string_ref& field, field_id id);

  column_meta& operator=(column_meta&& rhs) NOEXCEPT;
  column_meta& operator=(const column_meta&) = default;

  bool operator==(const column_meta& rhs) const;
  bool operator!=(const column_meta& rhs) const {
    return !(*this == rhs);
  }

  std::string name;
  field_id id{ type_limits<type_t::field_id_t>::invalid() };
}; // column_meta

NS_END

#endif

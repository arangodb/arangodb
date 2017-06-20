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

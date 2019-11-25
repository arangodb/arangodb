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

#ifndef IRESEARCH_TYPE_ID_H
#define IRESEARCH_TYPE_ID_H

#include "shared.hpp"
#include "string.hpp"

#include <functional>

NS_ROOT

struct IRESEARCH_API type_id {
  type_id() : hash(compute_hash(this)) { }

  bool operator==(const type_id& rhs) const noexcept {
    return this == &rhs;
  }

  bool operator!=(const type_id& rhs) const noexcept {
    return !(*this == rhs);
  }

  /* boost::hash_combile support */
  friend size_t hash_value(const type_id& type) { return type.hash; }
    
  operator const type_id*() const { return this; }

  size_t hash;

 private:
  static size_t compute_hash(const type_id* ptr) {
    return irs::hash_utils::hash(
      irs::string_ref(reinterpret_cast<const char*>(ptr), sizeof(ptr))
    );
  }
}; // type_id

#define DECLARE_TYPE_ID( type_id_name ) static const type_id_name& type()
#define DEFINE_TYPE_ID(class_name, type_id_name) const type_id_name& class_name::type() 

NS_END // root 

NS_BEGIN( std )

template<>
struct hash<iresearch::type_id> {
  typedef iresearch::type_id argument_type;
  typedef size_t result_type;

  result_type operator()( const argument_type& key ) const {
    return key.hash;
  }
};

NS_END // std

#endif

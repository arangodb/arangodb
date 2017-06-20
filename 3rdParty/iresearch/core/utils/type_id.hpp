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

#ifndef IRESEARCH_TYPE_ID_H
#define IRESEARCH_TYPE_ID_H

#include "shared.hpp"
#include <functional>

NS_ROOT

struct IRESEARCH_API type_id {
  type_id() : hash(std::hash<const type_id*>()(this)) { }

  bool operator==( const type_id& rhs ) const {
    return this == &rhs;
  }

  bool operator!=( const type_id& rhs ) const {
    return !( *this == rhs );
  }

  /* boost::hash_combile support */
  friend size_t hash_value( const type_id& type ) { return type.hash; }
    
  operator const type_id*() const { return this; }

  size_t hash;
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
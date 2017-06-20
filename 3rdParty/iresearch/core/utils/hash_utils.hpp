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

#ifndef IRESEARCH_HASH_UTILS_H
#define IRESEARCH_HASH_UTILS_H

#include "shared.hpp"
#include "string.hpp"

// -----------------------------------------------------------------------------
// --SECTION--                                                        hash utils
// -----------------------------------------------------------------------------

NS_ROOT

template<typename Elem>
class IRESEARCH_API_TEMPLATE hashed_basic_string_ref : public basic_string_ref<Elem> {
 public:
  typedef basic_string_ref<Elem> base_t;

  hashed_basic_string_ref(size_t hash, const base_t& ref)
    : base_t(ref), hash_(hash) {
  }
  
  hashed_basic_string_ref(size_t hash, const base_t& ref, size_t size)
    : base_t(ref, size), hash_(hash) {
  }
  
  hashed_basic_string_ref(size_t hash, const typename base_t::char_type* ptr)
    : base_t(ptr), hash_(hash) {
  }

  hashed_basic_string_ref(size_t hash, const typename base_t::char_type* ptr, size_t size)
    : base_t(ptr, size), hash_(hash) {
  }
  
  hashed_basic_string_ref(size_t hash, const std::basic_string<typename base_t::char_type>& str)
    : base_t(str), hash_(hash) {
  }
  
  hashed_basic_string_ref(size_t hash, const std::basic_string<typename base_t::char_type>& str, size_t size)
    : base_t(str, size), hash_(hash) {
  }

  inline size_t hash() const { return hash_; }

 private:
  size_t hash_;
}; // hashed_basic_string_ref 

template<typename Elem, typename Hasher>
hashed_basic_string_ref<Elem> make_hashed_ref(const basic_string_ref<Elem>& ref, const Hasher& hasher = Hasher()) {
  return hashed_basic_string_ref<Elem>(hasher(ref), ref);
}

template<typename Elem, typename Hasher>
hashed_basic_string_ref<Elem> make_hashed_ref(const basic_string_ref<Elem>& ref, size_t size, const Hasher& hasher = Hasher()) {
  return hashed_basic_string_ref<Elem>(hasher(ref), ref, size);
}

typedef hashed_basic_string_ref<byte_type> hashed_bytes_ref;
typedef hashed_basic_string_ref<char> hashed_string_ref;

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                    std extensions
// -----------------------------------------------------------------------------

NS_BEGIN(std)

template<>
struct hash<::iresearch::hashed_bytes_ref> {
  size_t operator()(const ::iresearch::hashed_bytes_ref& value) const {
    return value.hash();
  }
};

template<>
struct hash<::iresearch::hashed_string_ref> {
  size_t operator()(const ::iresearch::hashed_string_ref& value) const {
    return value.hash();
  }
};

NS_END // std

#endif
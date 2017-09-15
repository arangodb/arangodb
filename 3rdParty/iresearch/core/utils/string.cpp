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

#include <chrono>

#include "MurmurHash/MurmurHash3.h"

#include "string.hpp"

// -----------------------------------------------------------------------------
// --SECTION--                                                     hash function
// -----------------------------------------------------------------------------

NS_LOCAL

inline uint32_t get_seed() {
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::high_resolution_clock::now().time_since_epoch()
  );

  return static_cast<uint32_t>(ms.count());
}

template<typename T>
inline size_t get_hash(const T* value, size_t size) {
  static const auto seed = get_seed();
  uint32_t code;

  MurmurHash3_x86_32(value, size, seed, &code);

  return code;
}

NS_END

NS_ROOT

/* -------------------------------------------------------------------
 * basic_string_ref
 * ------------------------------------------------------------------*/

#if defined(_MSC_VER) && defined(IRESEARCH_DLL)

template class IRESEARCH_API basic_string_ref<char>;
template class IRESEARCH_API basic_string_ref<byte_type>;

#endif

NS_BEGIN(hash_utils)

size_t hash(const bstring& value) {
  return get_hash(value.c_str(), value.size());
}

size_t hash(const char* value) {
  return get_hash(value, std::char_traits<char>::length(value) * sizeof(char));
}

size_t hash(const wchar_t* value) {
  return get_hash(value, std::char_traits<wchar_t>::length(value) * sizeof(wchar_t));
}

size_t hash(const bytes_ref& value) {
  return get_hash(value.c_str(), value.size());
}

size_t hash(const string_ref& value) {
  return get_hash(value.c_str(), value.size());
}

NS_END // detail
NS_END

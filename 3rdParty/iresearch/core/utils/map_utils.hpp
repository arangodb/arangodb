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

#ifndef IRESEARCH_MAP_UTILS_H
#define IRESEARCH_MAP_UTILS_H

#include <cassert>
#include <tuple>

#include "shared.hpp"

NS_ROOT
NS_BEGIN(map_utils)

////////////////////////////////////////////////////////////////////////////
/// @brief helper for prior C++17 compilers (before MSVC 2015, GCC 6)
////////////////////////////////////////////////////////////////////////////
template<typename Container, typename... Args>
inline std::pair<typename Container::iterator, bool> try_emplace(
  Container& container,
  const typename Container::key_type& key,
  Args&&... args
) {
  #if (defined(_MSC_VER) && _MSC_VER >= 1900) \
       || (defined(__GNUC__) && __GNUC__ >= 6 && defined(__cpp_lib_unordered_map_try_emplace) && defined(__cpp_lib_map_try_emplace))
    return container.try_emplace(key, std::forward<Args>(args)...);
  #else
    const auto it = container.find(key);

    if (it != container.end()) {
      return{ it, false };
    }

    return container.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(key),
      std::forward_as_tuple(std::forward<Args>(args)...)
    );
  #endif
}

////////////////////////////////////////////////////////////////////////////
/// @brief helper for prior C++17 compilers (before MSVC 2015, GCC 6)
////////////////////////////////////////////////////////////////////////////
template<typename Container, typename... Args>
inline std::pair<typename Container::iterator, bool> try_emplace(
  Container& container,
  typename Container::key_type&& key,
  Args&&... args
) {
  #if (defined(_MSC_VER) && _MSC_VER >= 1900) \
       || (defined(__GNUC__) && __GNUC__ >= 6 && defined(__cpp_lib_unordered_map_try_emplace) && defined(__cpp_lib_map_try_emplace))
    return container.try_emplace(key, std::forward<Args>(args)...);
  #else
    const auto it = container.find(key);

    if (it != container.end()) {
      return{ it, false };
    }

    return container.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(key)),
      std::forward_as_tuple(std::forward<Args>(args)...)
    );
  #endif
}

////////////////////////////////////////////////////////////////////////////
/// @brief helper to update key after insertion of value
///  Note: use with caution since new hash and key most == old hash and key
////////////////////////////////////////////////////////////////////////////
template<typename Container, typename KeyGenerator, typename... Args>
inline std::pair<typename Container::iterator, bool>  try_emplace_update_key(
  Container& container,
  const KeyGenerator& generator,
  const typename Container::key_type& key,
  Args&&... args
) {
  auto res = try_emplace(container, key, std::forward<Args>(args)...);

  if (res.second) {
    auto& existing = const_cast<typename Container::key_type&>(res.first->first);

    #ifdef IRESEARCH_DEBUG
      auto generated = generator(res.first->first, res.first->second);
      assert(existing == generated);
      existing = generated;
    #else
      existing = generator(res.first->first, res.first->second);
    #endif // IRESEARCH_DEBUG
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////
/// @brief helper to update key after insertion of value
///  Note: use with caution since new hash and key most == old hash and key
////////////////////////////////////////////////////////////////////////////
template<typename Container, typename KeyGenerator, typename... Args>
inline std::pair<typename Container::iterator, bool>  try_emplace_update_key(
  Container& container,
  const KeyGenerator& generator,
  typename Container::key_type&& key,
  Args&&... args
) {
  auto res = try_emplace(container, std::move(key), std::forward<Args>(args)...);

  if (res.second) {
    auto& existing = const_cast<typename Container::key_type&>(res.first->first);

    #ifdef IRESEARCH_DEBUG
      auto generated = generator(res.first->first, res.first->second);
      assert(existing == generated);
      existing = generated;
    #else
      existing = generator(res.first->first, res.first->second);
    #endif // IRESEARCH_DEBUG
  }

  return res;
}

NS_END // map_utils
NS_END // NS_ROOT

#endif

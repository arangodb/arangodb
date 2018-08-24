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
///  Note: use with caution since new hash and key must == old hash and key
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
///  Note: use with caution since new hash and key must == old hash and key
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
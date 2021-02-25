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

namespace iresearch {
namespace map_utils {

////////////////////////////////////////////////////////////////////////////
/// @brief helper to update key after insertion of value
///  Note: use with caution since new hash and key must == old hash and key
////////////////////////////////////////////////////////////////////////////
template<typename Container,
         typename KeyGenerator,
         typename Key,
         typename... Args,
         typename = std::enable_if<
           std::is_same<typename Container::key_type, Key>::value
         >>
inline std::pair<typename Container::iterator, bool> try_emplace_update_key(
    Container& container, const KeyGenerator& generator,
    Key&& key, Args&&... args) {
  const auto res = container.try_emplace(std::forward<Key>(key),
                                         std::forward<Args>(args)...);

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

} // map_utils
} // namespace iresearch {

#endif

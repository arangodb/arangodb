////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_COMMON_H
#define ARANGODB_CACHE_COMMON_H

#include <cstdint>
#include <cstdlib>

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Common size for all bucket types.
////////////////////////////////////////////////////////////////////////////////
constexpr std::size_t BUCKET_SIZE = 128;

////////////////////////////////////////////////////////////////////////////////
/// @brief Enum to specify cache types.
////////////////////////////////////////////////////////////////////////////////
enum CacheType { Plain, Transactional };

////////////////////////////////////////////////////////////////////////////////
/// @brief Enum to allow easy statistic recording across classes.
////////////////////////////////////////////////////////////////////////////////
enum class Stat : std::uint8_t { findHit = 1, findMiss = 2 };

};  // end namespace cache
};  // end namespace arangodb

#endif

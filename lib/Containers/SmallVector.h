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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CONTAINERS_SMALL_VECTOR_H
#define ARANGODB_CONTAINERS_SMALL_VECTOR_H 1

#include <vector>

#include "Containers/details/short_alloc.h"

namespace arangodb {
namespace containers {

template <class T, std::size_t BufSize = 64, std::size_t ElementAlignment = alignof(T)>
using SmallVector = std::vector<T, detail::short_alloc<T, BufSize, ElementAlignment>>;

}  // namespace containers
}  // namespace arangodb

#endif

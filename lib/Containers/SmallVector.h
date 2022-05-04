////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#pragma once

#include <boost/container/small_vector.hpp>

#include <memory>
#include <cstddef>

namespace arangodb::containers {

template<typename T, std::size_t N, typename A = std::allocator<T>>
using SmallVector = boost::container::small_vector<T, N, A>;

// We cannot use absl::InlinedVector while they don't fix this issue:
// absl::InlinedVector::data() contains branch
// https://github.com/abseil/abseil-cpp/issues/1165

}  // namespace arangodb::containers

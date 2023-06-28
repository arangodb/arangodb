////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <boost/container/pmr/memory_resource.hpp>

namespace std_pmr {
using namespace boost::container::pmr;

using string =
    std::basic_string<char, std::char_traits<char>,
                      boost::container::pmr::polymorphic_allocator<char>>;
}  // namespace std_pmr

#include <absl/container/internal/hash_function_defaults.h>

namespace absl::container_internal {
template<>
struct HashEq<std_pmr::string> : BasicStringHashEq<char> {};
}  // namespace absl::container_internal

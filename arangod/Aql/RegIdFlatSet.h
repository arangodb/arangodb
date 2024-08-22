////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>

// Note: #include <boost/container/flat_set.hpp> to use the following types

namespace boost::container {
template<class T>
class new_allocator;
template<class Key, class Compare, class AllocatorOrContainer>
class flat_set;
}  // namespace boost::container

namespace arangodb {

namespace containers {
template<class Key, class Compare = std::less<Key>,
         class AllocatorOrContainer = boost::container::new_allocator<Key>>
using flat_set = boost::container::flat_set<Key, Compare, AllocatorOrContainer>;
}

namespace aql {
struct RegisterId;
using RegIdFlatSet = containers::flat_set<RegisterId>;
}  // namespace aql
}  // namespace arangodb

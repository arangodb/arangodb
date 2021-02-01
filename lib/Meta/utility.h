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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_META_UTILITY_H
#define ARANGODB_META_UTILITY_H 1

#include <memory>

namespace arangodb {
namespace meta {

////////////////////////////////////////////////////////////////////////////////
/// @brief adjusts constness of 'Out' according to 'In'
////////////////////////////////////////////////////////////////////////////////
template <typename In, typename Out>
struct adjustConst {
  typedef Out value_type;
  typedef Out& reference;
  typedef Out* pointer;
};

template <typename In, typename Out>
struct adjustConst<const In, Out> {
  typedef const Out value_type;
  typedef const Out& reference;
  typedef const Out* pointer;
};

template <class T, class U = T>
T exchange(T& obj, U&& new_value) {
  T old_value = std::move(obj);
  obj = std::forward<U>(new_value);
  return old_value;
}

}  // namespace meta
}  // namespace arangodb
#endif

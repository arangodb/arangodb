////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_TRANSACTION_COUNTTYPE_H
#define ARANGOD_TRANSACTION_COUNTTYPE_H 1

#include "Basics/Common.h"

namespace arangodb {
namespace transaction {

enum class CountType {
  // actual and accurate result. always returns the collection's actual count value
  Normal,
  // potentially return a cached result, if the cache value has not yet expired
  // may return an outdated value, but may save querying the collection
  TryCache,
  // always return cached result. will always return a stale result, but will
  // never query the collection's actual count
  ForceCache,
  // per-shard detailed results. will always query the actual counts
  Detailed
};

}
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_API_COLLECTIONS_H
#define ARANGOD_VOC_BASE_API_COLLECTIONS_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include "Basics/Result.h"
#include "VocBase/voc-types.h"

#include <functional>

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
namespace methods {

/// Common code for collection REST handler and v8-collections
struct Collections {
  static void enumerateCollections(
      TRI_vocbase_t* vocbase, std::function<void(LogicalCollection*)> const&);
  static bool lookupCollection(TRI_vocbase_t* vocbase,
                               std::string const& collection,
                               std::function<void(LogicalCollection*)> const&);
};
}
}

#endif

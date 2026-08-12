////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/StaticStrings.h"
#include "Inspection/Access.h"
#include "Inspection/Status.h"
#include "VocBase/Properties/InspectContexts.h"

#include <functional>
#include <cstdint>
#include <string>

namespace arangodb {

struct CollectionStorageProperties {
  struct Transformers {
    /// VPack stores objectId as a string; memory uses uint64_t (same as today).
    struct ObjectIdAsString {
      using MemoryType = uint64_t;
      using SerializedType = std::string;

      static inspection::Status toSerialized(MemoryType v,
                                             SerializedType& result);
      static inspection::Status fromSerialized(SerializedType const& v,
                                               MemoryType& result);
    };
  };

  /// RocksDB object id; 0 means not assigned yet / coordinator stub.
  /// cacheEnabled lives only on CollectionMutableProperties.
  uint64_t objectId{0};

  bool operator==(CollectionStorageProperties const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, CollectionStorageProperties& props) {
  auto objectIdField = std::invoke([&]() {
    if constexpr (isInternalContext<Inspector>) {
      // Markers store objectId as a number in some paths, and
      // RocksDBMetaCollection owns it anyway.
      return f.ignoreField(StaticStrings::ObjectId);
    } else {
      return f.field(StaticStrings::ObjectId, props.objectId)
          .transformWith(
              CollectionStorageProperties::Transformers::ObjectIdAsString{})
          .fallback(f.keep());
    }
  });

  return f.object(props).fields(std::move(objectIdField));
}

}  // namespace arangodb
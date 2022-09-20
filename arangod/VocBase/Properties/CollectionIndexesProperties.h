////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include "VocBase/voc-types.h"

namespace arangodb {

namespace velocypack {
class Builder;
}

struct CollectionIndexesProperties {
  // TODO: Create a inspectable struct for index infos.
  std::vector<velocypack::Builder> indexes;

  static CollectionIndexesProperties defaultIndexesForCollectionType(
      TRI_col_type_e type);
};

template<class Inspector>
auto inspect(Inspector& f, CollectionIndexesProperties& props) {
  return f.object(props).fields(f.field("indexes", props.indexes));
}

}  // namespace arangodb

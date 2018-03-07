////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "QueryResult.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the "extra" attribute values from the result.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> QueryResult::extra() const {
  // build "extra" attribute
  auto extra = std::make_shared<VPackBuilder>();
  try {
    VPackObjectBuilder b(extra.get());
    if (stats != nullptr) {
      VPackSlice const value = stats->slice();
      if (!value.isNone()) {
        extra->add("stats", value);
      }
    }
    if (profile != nullptr) {
      extra->add(VPackValue("profile"));
      extra->add(profile->slice());
    }
    if (warnings == nullptr) {
      extra->add("warnings", VPackSlice::emptyArraySlice());
    } else {
      extra->add(VPackValue("warnings"));
      extra->add(warnings->slice());
    }
  } catch (...) {
    return nullptr;
  }
  return extra;
}

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

#include "PlanCollection.h"
#include "Inspection/VPack.h"

#include <velocypack/Slice.h>

using namespace arangodb;

PlanCollection::PlanCollection() {}

ResultT<PlanCollection> PlanCollection::fromCreateAPIBody(VPackSlice input) {
  try {
    PlanCollection res;
    auto status = velocypack::deserializeWithStatus(input, res);
    if (status.ok()) {
      return res;
    }
    if (status.path().empty() || status.path() == "name") {
      // Special handling to be backwards compatible error reporting
      // on "name"
      return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
    }
    return Result{TRI_ERROR_BAD_PARAMETER,
                  status.error() + " on path " + status.path()};
  } catch (basics::Exception const& e) {
    return Result{e.code(), e.message()};
  } catch (std::exception const& e) {
    return Result{TRI_ERROR_INTERNAL, e.what()};
  }
}

arangodb::velocypack::Builder PlanCollection::toCollectionsCreate() {
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::serialize(builder, *this);
  return builder;
}

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "IndexFactory.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"

#include <velocypack/Slice.h>

using namespace arangodb;

TRI_idx_iid_t IndexFactory::validateSlice(arangodb::velocypack::Slice info, 
                                          bool generateKey, 
                                          bool isClusterConstructor) {
  if (!info.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  TRI_idx_iid_t iid = 0;
  arangodb::velocypack::Slice value = info.get("id");
  if (value.isString()) {
    iid = basics::StringUtils::uint64(value.copyString());
  } else if (value.isNumber()) {
    iid =
        basics::VelocyPackHelper::getNumericValue<TRI_idx_iid_t>(info, "id", 0);
  } else if (!generateKey) {
    // In the restore case it is forbidden to NOT have id
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot restore index without index identifier");
  }

  if (iid == 0 && !isClusterConstructor) {
    // Restore is not allowed to generate an id
    VPackSlice type = info.get("type");
    // dont generate ids for indexes of type "primary"
    // id 0 is expected for primary indexes
    if (!type.isString() || !type.isEqualString("primary")) {
      TRI_ASSERT(generateKey);
      iid = arangodb::Index::generateId();
    }
  }

  return iid;
}

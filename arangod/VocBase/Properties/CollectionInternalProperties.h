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

#include "Basics/StaticStrings.h"
#include "VocBase/voc-types.h"

#include <string>

namespace arangodb {

struct CollectionInternalProperties {
  std::string globallyUniqueId = StaticStrings::Empty;
  std::string id = StaticStrings::Empty;
  bool syncByRevision = true;
  bool usesRevisionsAsDocumentIds = true;
  bool isSmartChild = false;
  bool deleted = false;
  uint64_t internalValidatorType = 0;

  /* Still needed? Especially in Agency
  std::underlying_type<TRI_vocbase_col_status_e> status =
      TRI_vocbase_col_status_e::TRI_VOC_COL_STATUS_LOADED;
  */
};

template<class Inspector>
auto inspect(Inspector& f, CollectionInternalProperties& props) {
  return f.object(props).fields(
      f.field("globallyUniqueId", props.globallyUniqueId).fallback(f.keep()),
      f.field("id", props.id).fallback(f.keep()),
      f.field(StaticStrings::SyncByRevision, props.syncByRevision)
          .fallback(f.keep()),
      f.field(StaticStrings::UsesRevisionsAsDocumentIds,
              props.usesRevisionsAsDocumentIds)
          .fallback(f.keep()),
      f.field(StaticStrings::IsSmartChild, props.isSmartChild)
          .fallback(f.keep()),
      f.field("deleted", props.deleted).fallback(f.keep()),
      f.field(StaticStrings::InternalValidatorTypes,
              props.internalValidatorType)
          .fallback(f.keep()));
}

}  // namespace arangodb

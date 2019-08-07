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

#ifndef ARANGOD_COLLECTION_EXPORT_H
#define ARANGOD_COLLECTION_EXPORT_H 1

#include <unordered_set>

#include "Basics/Common.h"
#include "Basics/debugging.h"

namespace arangodb {

struct CollectionExport {
 public:
  struct Restrictions {
    enum Type { RESTRICTION_NONE, RESTRICTION_INCLUDE, RESTRICTION_EXCLUDE };

    Restrictions() : fields(), type(RESTRICTION_NONE) {}

    std::unordered_set<std::string> fields;
    Type type;
  };

  static bool IncludeAttribute(CollectionExport::Restrictions::Type const restrictionType,
                               std::unordered_set<std::string> const& fields,
                               std::string const& key) {
    if (restrictionType == CollectionExport::Restrictions::RESTRICTION_INCLUDE ||
        restrictionType == CollectionExport::Restrictions::RESTRICTION_EXCLUDE) {
      bool const keyContainedInRestrictions = (fields.find(key) != fields.end());
      if ((restrictionType == CollectionExport::Restrictions::RESTRICTION_INCLUDE &&
           !keyContainedInRestrictions) ||
          (restrictionType == CollectionExport::Restrictions::RESTRICTION_EXCLUDE &&
           keyContainedInRestrictions)) {
        // exclude the field
        return false;
      }
      // include the field
      return true;
    } else {
      // no restrictions
      TRI_ASSERT(restrictionType == CollectionExport::Restrictions::RESTRICTION_NONE);
      return true;
    }
    return true;
  }
};
}  // namespace arangodb

#endif

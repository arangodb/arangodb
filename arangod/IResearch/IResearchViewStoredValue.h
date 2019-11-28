////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_VIEW_STORED_VALUE_H
#define ARANGODB_IRESEARCH__IRESEARCH_VIEW_STORED_VALUE_H 1

#include "Basics/AttributeNameParser.h"
#include "Basics/debugging.h"

#include <velocypack/Slice.h>

namespace arangodb {

namespace velocypack {
class Builder;
}

/*
{"links" : {
            "mycol1" : {"fields" : {"str" : {"analyzers" : ["text_en"]}}, "includeAllFields" : true, "storeValues" : "value",
                       "storedFields": [["obj.foo.val1", "obj.foo.val2"], ["obj.bar.val1", "obj.bar.val2"]]}, // or string

            "mycol2" : {"fields" : {"str" : {"analyzers" : ["text_en"]}}, "includeAllFields" : true, "storeValues" : "value"}
           }
}
*/

namespace iresearch {

class IResearchViewStoredValue {
 public:
  using StoredColumn = std::vector<std::pair<std::string, std::vector<basics::AttributeName>>>;

  bool operator==(IResearchViewStoredValue const& rhs) const noexcept {
    return _storedColumns == rhs._storedColumns;
  }

  bool operator!=(IResearchViewStoredValue const& rhs) const noexcept {
    return !(*this == rhs);
  }

  std::vector<StoredColumn> const& columns() const noexcept {
    return _storedColumns;
  }

  size_t memory() const noexcept;

  bool empty() const noexcept {
    return _storedColumns.empty();
  }

  bool toVelocyPack(velocypack::Builder& builder) const;
  bool fromVelocyPack(velocypack::Slice, std::string& error);

 private:
  void clear() noexcept {
    _storedColumns.clear();
  }

  std::vector<StoredColumn> _storedColumns;
}; // IResearchViewStoredValue

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VIEW_STORED_VALUE_H

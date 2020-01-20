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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_VIEW_STORED_VALUES_H
#define ARANGODB_IRESEARCH__IRESEARCH_VIEW_STORED_VALUES_H 1

#include "Basics/AttributeNameParser.h"
#include "Basics/debugging.h"

#include <velocypack/Slice.h>

namespace arangodb {

namespace velocypack {
class Builder;
}

namespace iresearch {

class IResearchViewStoredValues {
 public:
  static const char FIELDS_DELIMITER;

  struct StoredColumn {
    std::string name;
    std::vector<std::pair<std::string, std::vector<basics::AttributeName>>> fields;

    bool operator==(StoredColumn const& rhs) const noexcept {
      return name == rhs.name;
    }

    bool operator!=(StoredColumn const& rhs) const noexcept {
      return !(*this == rhs);
    }
  };

  bool operator==(IResearchViewStoredValues const& rhs) const noexcept {
    return _storedColumns == rhs._storedColumns;
  }

  bool operator!=(IResearchViewStoredValues const& rhs) const noexcept {
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
}; // IResearchViewStoredValues

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VIEW_STORED_VALUES_H

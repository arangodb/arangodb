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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_VIEW_SORT_H
#define ARANGODB_IRESEARCH__IRESEARCH_VIEW_SORT_H 1

#include "Basics/AttributeNameParser.h"
#include "Basics/debugging.h"

#include <velocypack/Slice.h>

namespace arangodb {

namespace velocypack {
class Builder;
}

namespace iresearch {

// FIXME would be simpler to use instead:
//   typedef std::pair<std::vector<basics::AttributeName>, bool> SortEntry;
//   typedef std::vector<SortEntry> Sort;
// but currently SortCondition API is not ready for that
class IResearchViewSort {
 public:
  IResearchViewSort() = default;
  IResearchViewSort(const IResearchViewSort&) = default;
  IResearchViewSort(IResearchViewSort&&) = default;
  IResearchViewSort& operator=(const IResearchViewSort&) = default;
  IResearchViewSort& operator=(IResearchViewSort&&) = default;

  bool operator==(IResearchViewSort const& rhs) const noexcept {
    return _fields == rhs._fields && _directions == rhs._directions;
  }

  bool operator!=(IResearchViewSort const& rhs) const noexcept {
    return !(*this == rhs);
  }

  void clear() noexcept {
    _fields.clear();
    _directions.clear();
  }

  size_t size() const noexcept {
    TRI_ASSERT(_fields.size() == _directions.size());
    return _fields.size();
  }

  bool empty() const noexcept {
    TRI_ASSERT(_fields.size() == _directions.size());
    return _fields.empty();
  }

  void emplace_back(std::vector<basics::AttributeName>&& field, bool direction) {
    _fields.emplace_back(std::move(field));
    _directions.emplace_back(direction);
  }

  template<typename Visitor>
  bool visit(Visitor visitor) const {
    for (size_t i = 0, size = this->size(); i < size; ++i) {
      if (!visitor(_fields[i], _directions[i])) {
        return false;
      }
    }

    return true;
  }

  std::vector<std::vector<basics::AttributeName>> const& fields() const noexcept {
    return _fields;
  }

  std::vector<basics::AttributeName> const& field(size_t i) const noexcept {
    TRI_ASSERT(i < this->size());

    return _fields[i];
  }

  bool direction(size_t i) const noexcept {
    TRI_ASSERT(i < this->size());

    return _directions[i];
  }

  size_t memory() const noexcept;

  bool toVelocyPack(velocypack::Builder& builder) const;
  bool fromVelocyPack(velocypack::Slice, std::string& error);

 private:
  std::vector<std::vector<basics::AttributeName>> _fields;
  std::vector<bool> _directions;
}; // IResearchViewSort

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VIEW_SORT_H

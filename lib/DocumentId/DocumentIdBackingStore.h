////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include <string>
#include <optional>
#include <unordered_set>

#include "Basics/StringHeap.h"
#include "Containers/FlatHashSet.h"

#include "DocumentId/DocumentId.h"
#include "DocumentId/DocumentIdView.h"

#include "Basics/ResourceUsage.h"

namespace arangodb {

struct DocumentIdBackingStore {
  DocumentIdBackingStore(ResourceMonitor& monitor) : _heap(monitor, 8192){};

  DocumentIdBackingStore(DocumentIdBackingStore const&) = delete;
  DocumentIdBackingStore(DocumentIdBackingStore&&) = delete;
  auto operator=(DocumentIdBackingStore const&) = delete;
  auto operator=(DocumentIdBackingStore&&) = delete;

  auto addFromStringView(std::string_view id) -> std::optional<DocumentIdView> {
    auto v = DocumentIdView::fromStringView(id);
    if (!v.has_value()) {
      return std::nullopt;
    } else {
      return addFromDocumentIdView(v.value());
    }
  }
  auto addFromDocumentId(DocumentId id) -> DocumentIdView;
  auto addFromDocumentIdView(DocumentIdView id) -> DocumentIdView {
    auto elt = _knownDocumentIds.find(id);
    if (elt != _knownDocumentIds.end()) {
      return *elt;
    } else {
      auto s = _heap.registerString(id.idView());
      // TODO: this recalculates the separator position; i don't want to make it
      // readable, because that leaks the implementation detail as to how the id
      // is stored
      auto [r, _] =
          _knownDocumentIds.emplace(DocumentIdView::fromStringView(s).value());
      return *r;
    }
  }

  auto numberOfIds() const noexcept -> size_t {
    return _knownDocumentIds.size();
  }

 private:
  containers::FlatHashSet<DocumentIdView> _knownDocumentIds;
  StringHeap _heap;
};

}  // namespace arangodb

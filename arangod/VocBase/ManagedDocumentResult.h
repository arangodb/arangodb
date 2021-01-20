////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_VOC_BASE_MANAGED_DOCUMENT_RESULT_H
#define ARANGOD_VOC_BASE_MANAGED_DOCUMENT_RESULT_H 1

#include "Basics/Common.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

class ManagedDocumentResult {
 public:
  ManagedDocumentResult() : _revisionId(0) {}

  ManagedDocumentResult(ManagedDocumentResult const& other) = default;
  ManagedDocumentResult& operator=(ManagedDocumentResult const& other) = default;

  ManagedDocumentResult& operator=(ManagedDocumentResult&& other) noexcept {
    _string = std::move(other._string);
    _revisionId = other._revisionId;
    other.clear();
    return *this;
  }

  ManagedDocumentResult(ManagedDocumentResult&& other) noexcept
      : _string(std::move(other._string)),
        _revisionId(other._revisionId) {
    other.clear();
  }

  /// @brief copy in a valid document, sets revisionid
  void setManaged(uint8_t const* vpack);

  /// @brief access the internal buffer, revisionId must be set manually
  std::string* setManaged() noexcept {
    _string.clear();
    _revisionId = RevisionId::none();
    return &_string;
  }

  inline RevisionId revisionId() const noexcept { return _revisionId; }
  void setRevisionId(RevisionId rid) noexcept { _revisionId = rid; }
  void setRevisionId() noexcept;

  void clearData() noexcept {
    _string.clear();
  }
  
  void clear() noexcept {
    clearData();
    _revisionId = RevisionId::none();
  }
  
  inline uint8_t const* vpack() const noexcept {
    return reinterpret_cast<uint8_t const*>(_string.data());
  }

  inline bool empty() const noexcept {
    return _string.empty();
  }

  void addToBuilder(velocypack::Builder& builder) const;

 private:
  std::string _string;
  RevisionId _revisionId;
};

}  // namespace arangodb

#endif

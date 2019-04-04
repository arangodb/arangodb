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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_MANAGED_DOCUMENT_RESULT_H
#define ARANGOD_VOC_BASE_MANAGED_DOCUMENT_RESULT_H 1

#include "Basics/Common.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

class ManagedDocumentResult {
 public:
  ManagedDocumentResult() : _vpack(nullptr), _revisionId(0) {}

  ManagedDocumentResult(ManagedDocumentResult const& other) = default;
  ManagedDocumentResult& operator=(ManagedDocumentResult const& other) = default;

  ManagedDocumentResult& operator=(ManagedDocumentResult&& other) noexcept {
    _string = std::move(other._string);
    _vpack = other._vpack;
    _revisionId = other._revisionId;
    other.clear();
    return *this;
  }

  ManagedDocumentResult(ManagedDocumentResult&& other) noexcept
      : _string(std::move(other._string)),
        _vpack(other._vpack),
        _revisionId(other._revisionId) {
    other.clear();
  }

  /// @brief store pointer to a valid document
  void setUnmanaged(uint8_t const* vpack);
  /// @brief copy in a valid document
  void setManaged(uint8_t const* vpack);

  std::string* setManaged() noexcept {
    _string.clear();
    _vpack = nullptr;
    _revisionId = 0;
    return &_string;
  }

  inline TRI_voc_rid_t revisionId() const noexcept { return _revisionId; }
  void revisionId(TRI_voc_rid_t rid) noexcept { _revisionId = rid; }

  void clearData() noexcept {
    _string.clear();
    _vpack = nullptr;
  }
  
  void clear() noexcept {
    clearData();
    _revisionId = 0;
  }
  
  inline uint8_t const* vpack() const noexcept {
    if (_vpack == nullptr) {
      return reinterpret_cast<uint8_t const*>(_string.data());
    }
    return _vpack;
  }

  inline bool empty() const noexcept {
    return (_vpack == nullptr && _string.empty());
  }

  inline bool canUseInExternal() const noexcept {
    return _vpack != nullptr;
  }

  void addToBuilder(velocypack::Builder& builder, bool allowExternals) const;

 private:
  std::string _string;
  uint8_t* _vpack;
  TRI_voc_rid_t _revisionId;
};

}  // namespace arangodb

#endif

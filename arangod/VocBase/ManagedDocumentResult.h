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
  ManagedDocumentResult() :
    _vpack(nullptr),
    _managed(false) {}
  
  ManagedDocumentResult(ManagedDocumentResult const& other) = default;
  ManagedDocumentResult& operator=(ManagedDocumentResult const& other) = default;

  ManagedDocumentResult& operator=(ManagedDocumentResult&& other) noexcept {
    _string = std::move(other._string);
    _vpack = other._vpack;
    _localDocumentId = other._localDocumentId;
    _managed = other._managed;

    other.clear();
    return *this;
  }

  ManagedDocumentResult(ManagedDocumentResult&& other) noexcept 
    : _string(std::move(other._string)),
      _vpack(other._vpack),
      _localDocumentId(other._localDocumentId),
      _managed(other._managed) {
    other.clear();
  }

  void setUnmanaged(uint8_t const* vpack, LocalDocumentId const& documentId);

  void setManaged(uint8_t const* vpack, LocalDocumentId const& documentId);
  
  std::string* setManaged(LocalDocumentId const& documentId) {
    _string.clear();
    _vpack = nullptr;
    _localDocumentId = documentId;
    _managed = true;
    return &_string; 
  }
  
  inline LocalDocumentId localDocumentId() const { return _localDocumentId; }
  
  void clear() noexcept {
    _string.clear();
    _vpack = nullptr;
    _localDocumentId.clear();
    _managed = false;
  }
  
  inline uint8_t const* vpack() const {
    if (_managed) {
      return reinterpret_cast<uint8_t const*>(_string.data());
    }
    TRI_ASSERT(_vpack != nullptr);
    return _vpack;
  }

  inline bool empty() const {
    return (!_managed && _vpack == nullptr);
  }
   
  inline bool canUseInExternal() const {
    return !_managed;
  }
  
  void addToBuilder(velocypack::Builder& builder, bool allowExternals) const;

 private:
  std::string _string;
  uint8_t* _vpack;
  LocalDocumentId _localDocumentId;
  bool _managed;
};

}

#endif

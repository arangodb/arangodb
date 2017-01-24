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

namespace arangodb {

class ManagedDocumentResult {
 public:
  ManagedDocumentResult() : _vpack(nullptr), _lastRevisionId(0) {}
  ~ManagedDocumentResult() = default;

  inline uint8_t const* vpack() const { 
    TRI_ASSERT(_vpack != nullptr); 
    return _vpack; 
  }
  
  inline void addExisting(uint8_t const* vpack, TRI_voc_rid_t revisionId) {
    _vpack = vpack;
    _lastRevisionId = revisionId;
  }

  inline TRI_voc_rid_t lastRevisionId() const { return _lastRevisionId; }

  void clear() {
    _vpack = nullptr;
    _lastRevisionId = 0;
  }

 private:
  uint8_t const* _vpack;
  TRI_voc_rid_t _lastRevisionId;
};

}

#endif

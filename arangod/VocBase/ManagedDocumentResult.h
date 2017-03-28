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

#include "velocypack/Slice.h"
#include "velocypack/Buffer.h"
#include "velocypack/velocypack-aliases.h"
#include "Basics/Common.h"

namespace arangodb {

class ManagedDocumentResult {
 public:
  ManagedDocumentResult() : _length(0), _lastRevisionId(0), _vpack(nullptr),
                             _managed(false) {}
  ~ManagedDocumentResult() { reset(); }
  ManagedDocumentResult(ManagedDocumentResult const& other) = delete;
  ManagedDocumentResult& operator=(ManagedDocumentResult const& other) = delete;

  ManagedDocumentResult& operator=(ManagedDocumentResult&& other){
    if (other._managed){
      reset();
      _vpack = other._vpack;
      _length = other._length;
      _lastRevisionId = other._lastRevisionId;
      _managed = true;
      other._managed = false;
      other.reset();
    } else {
      setUnmanaged(other._vpack, other._lastRevisionId);
    }
    return *this;
  }

  ManagedDocumentResult(ManagedDocumentResult&& other) = delete;

  inline uint8_t const* vpack() const {
    TRI_ASSERT(_vpack != nullptr);
    return _vpack;
  }

  //add unmanaged vpack 
  inline void setUnmanaged(uint8_t const* vpack, TRI_voc_rid_t revisionId) {
    if(_managed) {
      reset();
    }
    TRI_ASSERT(_length == 0);
    _vpack = const_cast<uint8_t*>(vpack);
    _lastRevisionId = revisionId;
  }

  inline void setManaged(uint8_t const* vpack, TRI_voc_rid_t revisionId) {
    VPackSlice slice(vpack);
    auto newLen = slice.byteSize();
    if (_length >= newLen && _managed){
      std::memcpy(_vpack, vpack, newLen);
    } else {
      reset();
      _vpack = new uint8_t[newLen];
      std::memcpy(_vpack, vpack, newLen);
      _length=newLen;
    }
    _lastRevisionId = revisionId;
    _managed = true;
  }

  inline TRI_voc_rid_t lastRevisionId() const { return _lastRevisionId; }

  void reset() noexcept {
    if(_managed) {
      delete _vpack;
    }
    _vpack = nullptr;
    _lastRevisionId = 0;
    _managed = false;
    _length = 0;
  }

 private:
  uint64_t _length;
  TRI_voc_rid_t _lastRevisionId;
  uint8_t* _vpack;
  bool _managed;

};

}

#endif

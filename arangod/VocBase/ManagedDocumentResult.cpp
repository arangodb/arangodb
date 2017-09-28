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

#include "ManagedDocumentResult.h"
#include "Aql/AqlValue.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

void ManagedDocumentResult::clone(ManagedDocumentResult& cloned) const {
  cloned.reset();
  if (_useString) {
    cloned._useString = true;
    cloned._string = _string;
    cloned._lastRevisionId = _lastRevisionId;
    cloned._vpack = reinterpret_cast<uint8_t*>(const_cast<char*>(cloned._string.data()));
  } else if (_managed) {
    cloned.setManaged(_vpack, _lastRevisionId);
  } else {
    cloned.setUnmanaged(_vpack, _lastRevisionId);
  }
}

//add unmanaged vpack 
void ManagedDocumentResult::setUnmanaged(uint8_t const* vpack, TRI_voc_rid_t revisionId) {
  if(_managed || _useString) {
    reset();
  }
  TRI_ASSERT(_length == 0);
  _vpack = const_cast<uint8_t*>(vpack);
  _lastRevisionId = revisionId;
}

void ManagedDocumentResult::setManaged(uint8_t const* vpack, TRI_voc_rid_t revisionId) {
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

void ManagedDocumentResult::setManagedAfterStringUsage(TRI_voc_rid_t revisionId) {
  TRI_ASSERT(!_string.empty());
  TRI_ASSERT(_useString);
  
  _vpack = reinterpret_cast<uint8_t*>(const_cast<char*>(_string.data()));
  _lastRevisionId = revisionId;
  _useString = true;
}

void ManagedDocumentResult::setManaged(std::string&& str, TRI_voc_rid_t revisionId) {
  reset();
  _string = std::move(str);
  _vpack = reinterpret_cast<uint8_t*>(const_cast<char*>(_string.data()));
  _lastRevisionId = revisionId;
  _useString = true;
}

void ManagedDocumentResult::reset() noexcept {
  if (_managed) {
    delete[] _vpack;
  }
  _managed = false;
  _length = 0;

  if (_useString) {
    _string.clear();
    _useString = false;
  }

  _lastRevisionId = 0;
  _vpack = nullptr;
}

void ManagedDocumentResult::addToBuilder(velocypack::Builder& builder, bool allowExternals) const {
  TRI_ASSERT(!empty());
  auto slice = velocypack::Slice(_vpack);
  TRI_ASSERT(!slice.isExternal());
  if (allowExternals && canUseInExternal()) {
    builder.addExternal(slice.begin());
  } else {
    builder.add(slice);
  }
}

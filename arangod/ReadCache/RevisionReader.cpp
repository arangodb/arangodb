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

#include "RevisionReader.h"
#include "ReadCache/GlobalRevisionCacheChunk.h"

using namespace arangodb;

// constructor for a non-existing revision
RevisionReader::RevisionReader()
    : _chunk(nullptr), 
      _offset(0), 
      _version(0), 
      _ownsReference(false), 
      _ownsReader(false) {}

// constructor for an existing revision
RevisionReader::RevisionReader(GlobalRevisionCacheChunk* chunk, RevisionOffset offset, RevisionVersion version) 
    : _chunk(chunk), 
      _offset(offset), 
      _version(version), 
      _ownsReference(true), 
      _ownsReader(true) {
  TRI_ASSERT(_chunk != nullptr);
}

RevisionReader::~RevisionReader() {
  if (_ownsReference) {
    _chunk->removeReference();
  }
  if (_ownsReader) {
    _chunk->removeReader();
  }
}
 
// move constructor 
RevisionReader::RevisionReader(RevisionReader&& other) noexcept 
    : _chunk(other._chunk), 
      _offset(other._offset), 
      _version(other._version), 
      _ownsReference(other._ownsReference), 
      _ownsReader(other._ownsReader) {
  // other RevisionReader is not responsible for anything when we're done with it
  other._chunk = nullptr;
  other._ownsReference = false;
  other._ownsReader = false;
}

RevisionReader& RevisionReader::operator=(RevisionReader&& other) noexcept {
  if (this != &other) {
    _chunk = other._chunk;
    _offset = other._offset;
    _version = other._version;
    _ownsReference = other._ownsReference;
    _ownsReader = other._ownsReader;
  
    // other RevisionReader is not responsible for anything when we're done with it
    other._chunk = nullptr;
    other._ownsReference = false;
    other._ownsReader = false;
  }
  return *this;
}

arangodb::velocypack::Slice RevisionReader::revision() const {
  if (_chunk == nullptr) {
    // non-existing revision
    return arangodb::velocypack::Slice();
  }

  return arangodb::velocypack::Slice(_chunk->read(_offset.value + sizeof(uint64_t)));
}

uint64_t RevisionReader::collectionId() const {
  if (_chunk == nullptr) {
    // non-existing revision
    return 0;
  }

  return *reinterpret_cast<uint64_t const*>(_chunk->read(_offset.value));
}


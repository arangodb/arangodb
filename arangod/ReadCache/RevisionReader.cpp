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
#include "ReadCache/RevisionCacheChunk.h"

using namespace arangodb;

RevisionReader::RevisionReader(RevisionCacheChunk* chunk, RevisionOffset offset, RevisionVersion version) 
    : _chunk(chunk), _offset(offset), _version(version), _handled(false) {
  TRI_ASSERT(_chunk != nullptr);
}

RevisionReader::~RevisionReader() {
  if (!_handled) {
    _chunk->removeReference();
    _chunk->removeReader();
  }
}
  
RevisionReader::RevisionReader(RevisionReader&& other) noexcept 
    : _chunk(other._chunk), _offset(other._offset), _version(other._version), _handled(other._handled) {
  other._handled = true;
}

RevisionReader& RevisionReader::operator=(RevisionReader&& other) noexcept {
  if (this != &other) {
    _chunk = other._chunk;
    _offset = other._offset;
    _version = other._version;
    _handled = other._handled;

    other._handled = true;
  }
  return *this;
}

arangodb::velocypack::Slice RevisionReader::revision() const {
  return arangodb::velocypack::Slice(_chunk->read(_offset.value + sizeof(uint64_t)));
}

uint64_t RevisionReader::collectionId() const {
  return *reinterpret_cast<uint64_t const*>(_chunk->read(_offset.value));
}


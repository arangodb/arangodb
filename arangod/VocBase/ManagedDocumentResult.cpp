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

void ManagedDocumentResult::setUnmanaged(uint8_t const* vpack, LocalDocumentId const& documentId) {
  _string.clear();
  _vpack = const_cast<uint8_t*>(vpack);
  _localDocumentId = documentId;
  _managed = false;
}

void ManagedDocumentResult::setManaged(uint8_t const* vpack, LocalDocumentId const& documentId) {
  _string.assign(reinterpret_cast<char const*>(vpack), VPackSlice(vpack).byteSize());
  _vpack = nullptr;
  _localDocumentId = documentId;
  _managed = true;
}

void ManagedDocumentResult::setManagedAfterStringUsage(LocalDocumentId const& documentId) {
  _vpack = nullptr;
  _localDocumentId = documentId;
  _managed = true;
}

void ManagedDocumentResult::addToBuilder(velocypack::Builder& builder, bool allowExternals) const {
  uint8_t const* vpack;
  if (_managed) {
    vpack = reinterpret_cast<uint8_t const*>(_string.data());
  } else {
    vpack = _vpack;
  }
  TRI_ASSERT(vpack != nullptr);
  auto slice = velocypack::Slice(vpack);
  TRI_ASSERT(!slice.isExternal());
  if (allowExternals && canUseInExternal()) {
    builder.addExternal(slice.begin());
  } else {
    builder.add(slice);
  }
}

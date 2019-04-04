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
#include "Transaction/Helpers.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

void ManagedDocumentResult::setUnmanaged(uint8_t const* vpack) {
  _string.clear();
  _vpack = const_cast<uint8_t*>(vpack);
  _revisionId = transaction::helpers::extractRevFromDocument(VPackSlice(vpack));
}

void ManagedDocumentResult::setManaged(uint8_t const* vpack) {
  VPackSlice const slice(vpack);
  _string.assign(slice.startAs<char>(), slice.byteSize());
  _vpack = nullptr;
  _revisionId = transaction::helpers::extractRevFromDocument(slice);
}

void ManagedDocumentResult::addToBuilder(velocypack::Builder& builder,
                                         bool allowExternals) const {
  TRI_ASSERT(!empty());
  if (_vpack == nullptr) {
    TRI_ASSERT(!_string.empty());
    builder.add(VPackSlice(_string.data()));
  } else {
    if (allowExternals) {
      builder.addExternal(_vpack);
    } else {
      builder.add(VPackSlice(_vpack));
    }
  }
}

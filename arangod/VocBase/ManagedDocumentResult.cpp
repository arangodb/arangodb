////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

void ManagedDocumentResult::setManaged(uint8_t const* vpack) {
  VPackSlice const slice(vpack);
  _string.assign(slice.startAs<char>(), slice.byteSize());
  _revisionId = transaction::helpers::extractRevFromDocument(slice);
}

void ManagedDocumentResult::setRevisionId() noexcept {
  TRI_ASSERT(!this->empty());
  _revisionId = transaction::helpers::extractRevFromDocument(VPackSlice(this->vpack()));
}

void ManagedDocumentResult::addToBuilder(velocypack::Builder& builder) const {
  TRI_ASSERT(!empty());
  TRI_ASSERT(!_string.empty());
  builder.add(VPackSlice(reinterpret_cast<uint8_t const*>(_string.data())));
}

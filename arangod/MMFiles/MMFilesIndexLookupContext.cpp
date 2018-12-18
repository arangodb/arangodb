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

#include "MMFilesIndexLookupContext.h"
#include "MMFiles/MMFilesCollection.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

using namespace arangodb;

MMFilesIndexLookupContext::MMFilesIndexLookupContext(transaction::Methods* trx, 
                                       LogicalCollection* collection, 
                                       ManagedDocumentResult* result, 
                                       size_t numFields)
    : _trx(trx), 
      _collection(collection), 
      _result(result), 
      _numFields(numFields) {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(_collection != nullptr);
  // note: _result can be a nullptr
}
 
uint8_t const* MMFilesIndexLookupContext::lookup(LocalDocumentId token) const {
  TRI_ASSERT(_result != nullptr);
  try {
    if (static_cast<MMFilesCollection*>(_collection->getPhysical())->readDocument(_trx, token, *_result)) {
      return _result->vpack();
    } 
  } catch (...) {
  }
  return nullptr;
}

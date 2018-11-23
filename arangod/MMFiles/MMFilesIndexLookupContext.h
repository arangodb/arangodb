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

#ifndef ARANGOD_MMFILES_MMFILES_INDEX_LOOKUP_CONTEXT_H
#define ARANGOD_MMFILES_MMFILES_INDEX_LOOKUP_CONTEXT_H 1

#include "Basics/Common.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class LogicalCollection;
class ManagedDocumentResult;

namespace transaction {
class Methods;
}

class MMFilesIndexLookupContext {
 public:
  MMFilesIndexLookupContext() = delete;
  MMFilesIndexLookupContext(transaction::Methods* trx, 
                            LogicalCollection* collection, 
                            ManagedDocumentResult* result, 
                            size_t numFields);

  ~MMFilesIndexLookupContext() {}

  uint8_t const* lookup(LocalDocumentId token) const;

  ManagedDocumentResult* result() const { return _result; }

  inline size_t numFields() const { return _numFields; }

 private:
  transaction::Methods* _trx;
  LogicalCollection* _collection;
  mutable ManagedDocumentResult* _result;
  size_t const _numFields;
};

}

#endif

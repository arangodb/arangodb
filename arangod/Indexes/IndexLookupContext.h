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

#ifndef ARANGOD_INDEXES_INDEX_LOOKUP_CONTEXT_H
#define ARANGOD_INDEXES_INDEX_LOOKUP_CONTEXT_H 1

#include "Basics/Common.h"
#include "Logger/Logger.h"
#include "Utils/Transaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

namespace arangodb {

class IndexLookupContext {
 public:
  IndexLookupContext() = delete;
  IndexLookupContext(Transaction* trx, LogicalCollection* collection, ManagedDocumentResult* result, size_t numFields) 
      : _trx(trx), _collection(collection), _result(result), _numFields(numFields) {
    TRI_ASSERT(_trx != nullptr);
    TRI_ASSERT(_collection != nullptr);
    TRI_ASSERT(_result != nullptr);
  }

  ~IndexLookupContext() {}

  uint8_t const* lookup(TRI_voc_rid_t revisionId) {
    try {
      if (_collection->readRevision(_trx, *_result, revisionId)) {
        return _result->vpack();
      } 
    } catch (...) {
    }
    return nullptr;
  }

  ManagedDocumentResult* result() { return _result; }

  inline size_t numFields() const { return _numFields; }

 private:
  Transaction* _trx;
  LogicalCollection* _collection;
  ManagedDocumentResult* _result;
  size_t const _numFields;
};

}

#endif

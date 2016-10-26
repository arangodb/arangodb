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

#include "DocumentOperation.h"
#include "Indexes/IndexElement.h"
#include "Indexes/PrimaryIndex.h"
#include "Utils/Transaction.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::wal;
  
DocumentOperation::DocumentOperation(LogicalCollection* collection,
                                     TRI_voc_document_operation_e type)
      : _collection(collection),
        _tick(0),
        _type(type),
        _status(StatusType::CREATED) {
}

DocumentOperation::~DocumentOperation() {
  TRI_ASSERT(_status != StatusType::INDEXED);
 
  if (_status == StatusType::HANDLED) {
    try {
      if (_type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
          _type == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
        // remove old, now unused revision
        TRI_ASSERT(!_oldRevision.empty());
        TRI_ASSERT(!_newRevision.empty());
        _collection->removeRevision(_oldRevision._revisionId, true);
      } else if (_type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
        // remove old, now unused revision
        TRI_ASSERT(!_oldRevision.empty());
        TRI_ASSERT(_newRevision.empty());
        _collection->removeRevision(_oldRevision._revisionId, true);
      }
    } catch (...) {
      // never throw here because of destructor
    }
  } 
}
  
DocumentOperation* DocumentOperation::swap() {
  DocumentOperation* copy =
      new DocumentOperation(_collection, _type);
  copy->_tick = _tick;
  copy->_oldRevision = _oldRevision;
  copy->_newRevision = _newRevision;
  copy->_status = _status;

  _type = TRI_VOC_DOCUMENT_OPERATION_UNKNOWN;
  _status = StatusType::SWAPPED;
  _oldRevision.clear();
  _newRevision.clear();

  return copy;
}

void DocumentOperation::setVPack(uint8_t const* vpack) {
  TRI_ASSERT(!_newRevision.empty());
  _newRevision._vpack = vpack;
}

void DocumentOperation::setRevisions(DocumentDescriptor const& oldRevision,
                                     DocumentDescriptor const& newRevision) {
  TRI_ASSERT(_oldRevision.empty());
  TRI_ASSERT(_newRevision.empty());

  if (_type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    TRI_ASSERT(oldRevision.empty());
    TRI_ASSERT(!newRevision.empty());
    _oldRevision.clear();
    _newRevision.reset(newRevision);
  } else if (_type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
             _type == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    TRI_ASSERT(!oldRevision.empty());
    TRI_ASSERT(!newRevision.empty());
    _oldRevision.reset(oldRevision);
    _newRevision.reset(newRevision);
  } else if (_type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    TRI_ASSERT(!oldRevision.empty());
    TRI_ASSERT(newRevision.empty());
    _oldRevision.reset(oldRevision);
    _newRevision.clear();
  }
}

void DocumentOperation::revert(arangodb::Transaction* trx) {
  TRI_ASSERT(trx != nullptr);

  if (_status == StatusType::CREATED || 
      _status == StatusType::SWAPPED ||
      _status == StatusType::REVERTED) {
    return;
  }
  
  TRI_ASSERT(_status == StatusType::INDEXED || _status == StatusType::HANDLED);
  
  // set to reverted now
  _status = StatusType::REVERTED;

  TRI_voc_rid_t oldRevisionId = 0;
  VPackSlice oldDoc;
  if (_type != TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    TRI_ASSERT(!_oldRevision.empty());
    oldRevisionId = _oldRevision._revisionId;
    oldDoc = VPackSlice(_oldRevision._vpack);
  }

  TRI_voc_rid_t newRevisionId = 0;
  VPackSlice newDoc;
  if (_type != TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    TRI_ASSERT(!_newRevision.empty());
    newRevisionId = _newRevision._revisionId;
    newDoc = VPackSlice(_newRevision._vpack);
  }

  try {
    _collection->rollbackOperation(trx, _type, oldRevisionId, oldDoc, newRevisionId, newDoc);
  } catch (...) {
    // TODO: decide whether we should rethrow here
  }

  if (_type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    TRI_ASSERT(_oldRevision.empty());
    TRI_ASSERT(!_newRevision.empty());
    // remove now obsolete new revision
    try {
      _collection->removeRevision(newRevisionId, true);
    } catch (...) {
      // operation probably was never inserted
      // TODO: decide whether we should rethrow here
    }
  } else if (_type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
             _type == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    TRI_ASSERT(!_oldRevision.empty());
    TRI_ASSERT(!_newRevision.empty());
    SimpleIndexElement* element = _collection->primaryIndex()->lookupKeyRef(trx, Transaction::extractKeyFromDocument(newDoc));
    if (element != nullptr && element->revisionId() != 0) {
      VPackSlice keySlice(Transaction::extractKeyFromDocument(oldDoc));
      element->updateRevisionId(oldRevisionId, static_cast<uint32_t>(keySlice.begin() - oldDoc.begin()));
    }
    
    // remove now obsolete new revision
    try {
      _collection->removeRevision(newRevisionId, true);
    } catch (...) {
      // operation probably was never inserted
      // TODO: decide whether we should rethrow here
    }
  }
}


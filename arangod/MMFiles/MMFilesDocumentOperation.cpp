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

#include "MMFilesDocumentOperation.h"
#include "Indexes/IndexIterator.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
  
MMFilesDocumentOperation::MMFilesDocumentOperation(LogicalCollection* collection,
                                                   TRI_voc_document_operation_e type)
      : _collection(collection),
        _tick(0),
        _type(type),
        _status(StatusType::CREATED) {
}

MMFilesDocumentOperation::~MMFilesDocumentOperation() {}
  
MMFilesDocumentOperation* MMFilesDocumentOperation::clone() {
  MMFilesDocumentOperation* copy =
      new MMFilesDocumentOperation(_collection, _type);
  copy->_tick = _tick;
  copy->_oldRevision = _oldRevision;
  copy->_newRevision = _newRevision;
  copy->_status = _status;
  return copy;
}

void MMFilesDocumentOperation::swapped() {
  _type = TRI_VOC_DOCUMENT_OPERATION_UNKNOWN;
  _status = StatusType::SWAPPED;
  _oldRevision.clear();
  _newRevision.clear();
}

void MMFilesDocumentOperation::setVPack(uint8_t const* vpack) {
  TRI_ASSERT(!_newRevision.empty());
  _newRevision._vpack = vpack;
}

void MMFilesDocumentOperation::setDocumentIds(MMFilesDocumentDescriptor const& oldRevision,
                                            MMFilesDocumentDescriptor const& newRevision) {
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

void MMFilesDocumentOperation::revert(transaction::Methods* trx) {
  TRI_ASSERT(trx != nullptr);
  
  if (_status == StatusType::SWAPPED || _status == StatusType::REVERTED) {
    return;
  }

  // fetch old status and set it to reverted now
  StatusType status = _status;
  _status = StatusType::REVERTED;

  LocalDocumentId oldDocumentId;
  VPackSlice oldDoc;
  if (_type != TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    TRI_ASSERT(!_oldRevision.empty());
    oldDocumentId = _oldRevision._localDocumentId;
    oldDoc = VPackSlice(_oldRevision._vpack);
  }

  LocalDocumentId newDocumentId;
  VPackSlice newDoc;
  if (_type != TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    TRI_ASSERT(!_newRevision.empty());
    newDocumentId = _newRevision._localDocumentId;
    newDoc = VPackSlice(_newRevision._vpack);
  }

  auto physical = static_cast<MMFilesCollection*>(_collection->getPhysical());
  TRI_ASSERT(physical != nullptr);

  if (_type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    TRI_ASSERT(_oldRevision.empty());
    TRI_ASSERT(!_newRevision.empty());
    
    if (status != StatusType::CREATED) { 
      // remove document from indexes
      try {
        physical->rollbackOperation(
          *trx, _type, oldDocumentId, oldDoc, newDocumentId, newDoc
        );
      } catch (...) {
      }
    }

    // remove now obsolete new document
    try {
      physical->removeLocalDocumentId(newDocumentId, true);
    } catch (...) {
      // operation probably was never inserted
    }
  } else if (_type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
             _type == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    TRI_ASSERT(!_oldRevision.empty());
    TRI_ASSERT(!_newRevision.empty());
    
    try {
      // re-insert the old document
      physical->insertLocalDocumentId(_oldRevision._localDocumentId, _oldRevision._vpack, 0, true, true);
    } catch (...) {
    }

    if (status != StatusType::CREATED) { 
      try {
        // restore the old index state
        physical->rollbackOperation(
          *trx, _type, oldDocumentId, oldDoc, newDocumentId, newDoc
        );
      } catch (...) {
      }
    }
   
    // let the primary index entry point to the correct document
    MMFilesSimpleIndexElement* element = physical->primaryIndex()->lookupKeyRef(
        trx, transaction::helpers::extractKeyFromDocument(newDoc));

    if (element != nullptr && element->isSet()) {
      VPackSlice keySlice(transaction::helpers::extractKeyFromDocument(oldDoc));

      element->updateLocalDocumentId(oldDocumentId, static_cast<uint32_t>(keySlice.begin() - oldDoc.begin()));
    }

    physical->updateLocalDocumentId(oldDocumentId, oldDoc.begin(), 0, false);

    // remove now obsolete new document
    if (oldDocumentId != newDocumentId) { 
      // we need to check for the same document id here
      try {
        physical->removeLocalDocumentId(newDocumentId, true);
      } catch (...) {
      }
    }
  } else if (_type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    TRI_ASSERT(!_oldRevision.empty());
    TRI_ASSERT(_newRevision.empty());

    try {
      physical->insertLocalDocumentId(_oldRevision._localDocumentId, _oldRevision._vpack, 0, true, true);
    } catch (...) {
    }

    if (status != StatusType::CREATED) { 
      try {
        // remove from indexes again
        physical->rollbackOperation(
          *trx, _type, oldDocumentId, oldDoc, newDocumentId, newDoc
        );
      } catch (...) {
      }
    }
  }
}

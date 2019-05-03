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

#ifndef ARANGOD_MMFILES_DOCUMENT_OPERATION_H
#define ARANGOD_MMFILES_DOCUMENT_OPERATION_H 1

#include "Basics/Common.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class LogicalCollection;
namespace transaction {
class Methods;
}

struct MMFilesDocumentDescriptor {
  MMFilesDocumentDescriptor() : _localDocumentId(), _vpack(nullptr) {}
  MMFilesDocumentDescriptor(LocalDocumentId const& documentId, uint8_t const* vpack)
      : _localDocumentId(documentId), _vpack(vpack) {}

  bool empty() const { return _vpack == nullptr; }

  void reset(MMFilesDocumentDescriptor const& other) {
    _localDocumentId = other._localDocumentId;
    _vpack = other._vpack;
  }

  void clear() {
    _localDocumentId.clear();
    _vpack = nullptr;
  }

  LocalDocumentId _localDocumentId;
  uint8_t const* _vpack;
};

struct MMFilesDocumentOperation {
  enum class StatusType : uint8_t {
    CREATED,
    INDEXED,
    HANDLED,
    SWAPPED,
    REVERTED
  };

  MMFilesDocumentOperation(LogicalCollection* collection, TRI_voc_document_operation_e type);

  ~MMFilesDocumentOperation();

  MMFilesDocumentOperation* clone();
  void swapped();

  void setDocumentIds(MMFilesDocumentDescriptor const& oldRevision,
                      MMFilesDocumentDescriptor const& newRevision);

  void setVPack(uint8_t const* vpack);

  TRI_voc_document_operation_e type() const { return _type; }

  LogicalCollection* collection() const { return _collection; }

  void indexed() noexcept {
    TRI_ASSERT(_status == StatusType::CREATED);
    _status = StatusType::INDEXED;
  }

  void handled() noexcept {
    TRI_ASSERT(!_oldRevision.empty() || !_newRevision.empty());
    TRI_ASSERT(_status == StatusType::INDEXED);

    _status = StatusType::HANDLED;
  }

  void revert(transaction::Methods*);

 private:
  LogicalCollection* _collection;
  MMFilesDocumentDescriptor _oldRevision;
  MMFilesDocumentDescriptor _newRevision;
  TRI_voc_document_operation_e _type;
  StatusType _status;
};

}  // namespace arangodb

#endif

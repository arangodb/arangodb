
#ifndef ARANGOD_WAL_DOCUMENT_OPERATION_H
#define ARANGOD_WAL_DOCUMENT_OPERATION_H 1

#include "Basics/Common.h"
#include "VocBase/document-collection.h"
#include "VocBase/headers.h"
#include "VocBase/voc-types.h"
#include "Wal/Marker.h"

struct TRI_transaction_collection_s;

namespace arangodb {
class Transaction;

namespace wal {
class Marker;

struct DocumentOperation {
  enum class StatusType : uint8_t {
    CREATED,
    INDEXED,
    HANDLED,
    SWAPPED,
    REVERTED
  };

  DocumentOperation(arangodb::Transaction* trx, Marker* marker, bool freeMarker,
                    TRI_document_collection_t* document,
                    TRI_voc_document_operation_e type, TRI_voc_rid_t rid)
      : trx(trx),
        marker(marker),
        document(document),
        header(nullptr),
        rid(rid),
        tick(0),
        type(type),
        status(StatusType::CREATED),
        freeMarker(freeMarker) {
    TRI_ASSERT(marker != nullptr);
  }

  ~DocumentOperation() {
    if (status == StatusType::HANDLED) {
      if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
        document->_headersPtr->release(header, false);  // PROTECTED by trx
      }
    } else if (status != StatusType::REVERTED) {
      revert();
    }

    if (marker != nullptr && freeMarker) {
      delete marker;
    }
  }

  DocumentOperation* swap() {
    DocumentOperation* copy =
        new DocumentOperation(trx, marker, freeMarker, document, type, rid);
    copy->tick = tick;
    copy->header = header;
    copy->oldHeader = oldHeader;
    copy->status = status;

    type = TRI_VOC_DOCUMENT_OPERATION_UNKNOWN;
    freeMarker = false;
    marker = nullptr;
    header = nullptr;
    status = StatusType::SWAPPED;

    return copy;
  }

  void init() {
    if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
        type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
      // copy the old header into a safe area
      TRI_ASSERT(header != nullptr);
      oldHeader = *header;
    }
  }

  void indexed() {
    TRI_ASSERT(status == StatusType::CREATED);
    status = StatusType::INDEXED;
  }

  void handle() {
    TRI_ASSERT(header != nullptr);
    TRI_ASSERT(status == StatusType::INDEXED);

    if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
      // move header to the end of the list
      document->_headersPtr->moveBack(header, &oldHeader);  // PROTECTED by trx
    }

    // free the local marker buffer
    marker->freeBuffer();
    status = StatusType::HANDLED;
  }

  void revert() {
    if (header == nullptr || status == StatusType::SWAPPED ||
        status == StatusType::REVERTED) {
      return;
    }

    if (status == StatusType::INDEXED || status == StatusType::HANDLED) {
      TRI_RollbackOperationDocumentCollection(trx, document, type, header,
                                              &oldHeader);
    }

    if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
      document->_headersPtr->release(header, true);  // PROTECTED by trx
    } else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
      if (status != StatusType::CREATED && status != StatusType::INDEXED) {
        document->_headersPtr->move(header, &oldHeader);  // PROTECTED by trx in trxCollection
      }
      header->copy(oldHeader);
    } else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
      if (status != StatusType::CREATED) {
        document->_headersPtr->relink(header, &oldHeader);  // PROTECTED by trx
      }
    }

    status = StatusType::REVERTED;
  }

  arangodb::Transaction* trx;
  Marker* marker;
  TRI_document_collection_t* document;
  TRI_doc_mptr_t* header;
  TRI_doc_mptr_copy_t oldHeader;
  TRI_voc_rid_t const rid;
  TRI_voc_tick_t tick;
  TRI_voc_document_operation_e type;
  StatusType status;
  bool freeMarker;
};
}
}

#endif

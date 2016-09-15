
#ifndef ARANGOD_WAL_DOCUMENT_OPERATION_H
#define ARANGOD_WAL_DOCUMENT_OPERATION_H 1

#include "Basics/Common.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class Transaction;

namespace wal {

struct DocumentOperation {
  enum class StatusType : uint8_t {
    CREATED,
    INDEXED,
    HANDLED,
    SWAPPED,
    REVERTED
  };
  
  DocumentOperation(arangodb::Transaction* trx, 
                    LogicalCollection* collection,
                    TRI_voc_document_operation_e type)
      : trx(trx),
        collection(collection),
        header(nullptr),
        tick(0),
        type(type),
        status(StatusType::CREATED) {
  }

  ~DocumentOperation() {
    if (status == StatusType::HANDLED) {
      if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
        collection->releaseMasterpointer(header);
      }
    } else if (status != StatusType::REVERTED) {
      revert();
    }
  }

  DocumentOperation* swap() {
    DocumentOperation* copy =
        new DocumentOperation(trx, collection, type);
    copy->tick = tick;
    copy->header = header;
    copy->oldHeader = oldHeader;
    copy->status = status;

    type = TRI_VOC_DOCUMENT_OPERATION_UNKNOWN;
    header = nullptr;
    status = StatusType::SWAPPED;

    return copy;
  }

  void init() {
    if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
        type == TRI_VOC_DOCUMENT_OPERATION_REPLACE ||
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

    status = StatusType::HANDLED;
  }

  void revert() {
    if (header == nullptr || status == StatusType::SWAPPED ||
        status == StatusType::REVERTED) {
      return;
    }

    if (status == StatusType::INDEXED || status == StatusType::HANDLED) {
      collection->rollbackOperation(trx, type, header, &oldHeader);
    }

    if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
      collection->releaseMasterpointer(header);
    } else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
               type == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
      header->copy(oldHeader);
    }

    status = StatusType::REVERTED;
  }

  arangodb::Transaction* trx;
  LogicalCollection* collection;
  TRI_doc_mptr_t* header;
  TRI_doc_mptr_t oldHeader;
  TRI_voc_tick_t tick;
  TRI_voc_document_operation_e type;
  StatusType status;
};
}
}

#endif

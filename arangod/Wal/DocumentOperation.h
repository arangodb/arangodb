
#ifndef ARANGOD_WAL_DOCUMENT_OPERATION_H
#define ARANGOD_WAL_DOCUMENT_OPERATION_H 1

#include "Basics/Common.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/voc-types.h"
#include "Wal/Marker.h"

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
  
  DocumentOperation(arangodb::Transaction* trx, Marker const* marker,
                    LogicalCollection* collection,
                    TRI_voc_document_operation_e type)
      : trx(trx),
        marker(marker),
        collection(collection),
        header(nullptr),
        tick(0),
        rid(0),
        type(type),
        status(StatusType::CREATED) {
    TRI_ASSERT(marker != nullptr);
    VPackSlice s(static_cast<uint8_t*>(marker->vpack()));
    bool isOld;
    rid = TRI_StringToRid(s.get(StaticStrings::RevString).copyString(), isOld);
  }

  ~DocumentOperation() {
    if (status == StatusType::HANDLED) {
      if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
        collection->_masterPointers.release(header); 
      }
    } else if (status != StatusType::REVERTED) {
      revert();
    }
  }

  DocumentOperation* swap() {
    DocumentOperation* copy =
        new DocumentOperation(trx, marker, collection, type);
    copy->tick = tick;
    copy->rid = rid;
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
      collection->_masterPointers.release(header);
    } else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
               type == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
      header->copy(oldHeader);
    }

    status = StatusType::REVERTED;
  }

  arangodb::Transaction* trx;
  Marker const* marker;
  LogicalCollection* collection;
  TRI_doc_mptr_t* header;
  TRI_doc_mptr_t oldHeader;
  TRI_voc_tick_t tick;
  TRI_voc_rid_t rid;
  TRI_voc_document_operation_e type;
  StatusType status;
};
}
}

#endif

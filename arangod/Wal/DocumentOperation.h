
#ifndef TRIAGENS_VOC_BASE_DOCUMENT_OPERATION_H
#define TRIAGENS_VOC_BASE_DOCUMENT_OPERATION_H 1

#include "Basics/Common.h"
#include "VocBase/voc-types.h"
#include "VocBase/document-collection.h"
#include "Wal/Marker.h"
    
struct TRI_transaction_collection_s;

namespace triagens {
  namespace wal {
    class Marker;

    struct DocumentOperation {
      DocumentOperation (Marker* marker,
                         struct TRI_transaction_collection_s* trxCollection,
                         TRI_voc_document_operation_e type,
                         TRI_voc_rid_t rid) 
        : marker(marker),
          trxCollection(trxCollection),
          header(nullptr),
          type(type),
          rid(rid),
          handled(false) {

        assert(marker != nullptr);
      }

      ~DocumentOperation () {
        if (handled) {
          if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
            TRI_document_collection_t* document = trxCollection->_collection->_collection;
            document->_headersPtr->release(document->_headersPtr, header, false);  // PROTECTED by trx in trxCollection
          }
        }
        else {
          revert();
        }

        if (marker != nullptr) {
          delete marker;
        }
      }

      DocumentOperation* swap () {
        DocumentOperation* copy = new DocumentOperation(marker, trxCollection, type, rid);
        copy->header = header;
        copy->oldHeader = oldHeader;

        type = TRI_VOC_DOCUMENT_OPERATION_UNKNOWN; 
        marker = nullptr;
        handled = true;

        return copy;
      }

      void init () {
        if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
          // copy the old header into a safe area
          assert(header != nullptr);

          oldHeader = *header;
        }
      }

      void handle () {
        assert(header != nullptr);
        assert(! handled);
        
        TRI_document_collection_t* document = trxCollection->_collection->_collection;

        if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
          // nothing to do for insert
        }
        else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
          // move header to the end of the list
          document->_headersPtr->moveBack(document->_headersPtr, header, &oldHeader);  // PROTECTED by trx in trxCollection
        }
        else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
          // unlink the header
          document->_headersPtr->unlink(document->_headersPtr, header);  // PROTECTED by trx in trxCollection
        }
 
        // free the local marker buffer 
        delete[] marker->steal();
        handled = true;
      }

      void revert () {
        assert(header != nullptr);
        
        TRI_document_collection_t* document = trxCollection->_collection->_collection;
        TRI_RollbackOperationDocumentCollection(document, type, header, &oldHeader);

        if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
          document->_headersPtr->release(document->_headersPtr, header, true);  // PROTECTED by trx in trxCollection
        }
        else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
          document->_headersPtr->move(document->_headersPtr, header, &oldHeader);  // PROTECTED by trx in trxCollection
        }
        else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
          document->_headersPtr->relink(document->_headersPtr, header, header);  // PROTECTED by trx in trxCollection
        }
      }

      Marker* marker;
      struct TRI_transaction_collection_s* trxCollection;
      TRI_doc_mptr_t* header;
      TRI_doc_mptr_t oldHeader;
      TRI_voc_document_operation_e type;
      TRI_voc_rid_t const rid;
      bool handled;
    };
  }
}

#endif


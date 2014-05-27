
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
          rid(rid) {

        assert(marker != nullptr);
      }

      ~DocumentOperation () {
        if (marker != nullptr) {
          delete marker;
        }
      }

      void init () {
        if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
          // copy the old header into a safe area
          assert(header != nullptr);

          oldHeader = *header;
        }
      }

      void handled (bool isSingleOperationTransaction) {
        assert(header != nullptr);
        
        TRI_document_collection_t* document = (TRI_document_collection_t*) trxCollection->_collection->_collection;

        if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
          // nothing to do for insert
        }
        else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
          // move header to the end of the list
          document->_headers->moveBack(document->_headers, header, &oldHeader);
        }
        else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
          // free or unlink the header
          if (isSingleOperationTransaction) {
            document->_headers->release(document->_headers, header, true);
          }
          else {
            document->_headers->unlink(document->_headers, header);
          }
        }
 
        // free the local marker buffer 
        delete[] marker->steal();
      }

      void revert (bool isSingleOperationTransaction) {
        assert(header != nullptr);
        
        TRI_document_collection_t* document = (TRI_document_collection_t*) trxCollection->_collection->_collection;

        if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
          document->_headers->release(document->_headers, header, true);
        }
        else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
          document->_headers->move(document->_headers, header, &oldHeader);
        }
        else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
          document->_headers->relink(document->_headers, header, header);
        }
      }

      Marker* marker;
      struct TRI_transaction_collection_s* trxCollection;
      TRI_doc_mptr_t* header;
      TRI_doc_mptr_t oldHeader;
      TRI_voc_document_operation_e const type;
      TRI_voc_rid_t const rid;
    };
  }
}

#endif


////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_HANDLER_REST_VOCBASE_BASE_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_VOCBASE_BASE_HANDLER_H 1

#include "Basics/Common.h"

#include "Admin/RestBaseHandler.h"

#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/json-utilities.h"

#include "Rest/HttpResponse.h"
#include "Utils/transactions.h"
#include "RestServer/VocbaseContext.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_document_collection_t;
struct TRI_vocbase_col_s;
struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                      class RestVocbaseBaseHandler
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

    class VocbaseContext;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base request handler
////////////////////////////////////////////////////////////////////////////////

    class RestVocbaseBaseHandler : public admin::RestBaseHandler {
      private:
        RestVocbaseBaseHandler (RestVocbaseBaseHandler const&);
        RestVocbaseBaseHandler& operator= (RestVocbaseBaseHandler const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief batch path
////////////////////////////////////////////////////////////////////////////////

        static const std::string BATCH_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief document path
////////////////////////////////////////////////////////////////////////////////

        static const std::string DOCUMENT_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief document import path
////////////////////////////////////////////////////////////////////////////////

        static const std::string DOCUMENT_IMPORT_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge path
////////////////////////////////////////////////////////////////////////////////

        static const std::string EDGE_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief replication path
////////////////////////////////////////////////////////////////////////////////

        static const std::string REPLICATION_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief upload path
////////////////////////////////////////////////////////////////////////////////

        static const std::string UPLOAD_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief name of the queue
////////////////////////////////////////////////////////////////////////////////

        static const std::string QUEUE_NAME;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestVocbaseBaseHandler (rest::HttpRequest*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~RestVocbaseBaseHandler ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection needs to be created on the fly
///
/// this method will check the "createCollection" attribute of the request. if
/// it is set to true, it will verify that the named collection actually exists.
/// if the collection does not yet exist, it will create it on the fly.
/// if the "createCollection" attribute is not set or set to false, nothing will
/// happen, and the collection name will not be checked
////////////////////////////////////////////////////////////////////////////////

        bool checkCreateCollection (std::string const&,
                                    TRI_col_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a HTTP 201 or 202 response
////////////////////////////////////////////////////////////////////////////////

        void generate20x (rest::HttpResponse::HttpResponseCode,
                          std::string const&,
                          TRI_voc_key_t,
                          TRI_voc_rid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates ok message without content
////////////////////////////////////////////////////////////////////////////////

        void generateOk () {
          _response = createResponse(rest::HttpResponse::NO_CONTENT);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates ok message with no body but with certain status code
////////////////////////////////////////////////////////////////////////////////

        void generateOk (rest::HttpResponse::HttpResponseCode code) {
          _response = createResponse(code);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates message for a saved document
////////////////////////////////////////////////////////////////////////////////

        void generateSaved (triagens::arango::SingleCollectionWriteTransaction<1>& trx,
                            TRI_voc_cid_t cid,
                            TRI_doc_mptr_copy_t const& mptr) {
          TRI_ASSERT(mptr.getDataPtr() != nullptr); // PROTECTED by trx here

          rest::HttpResponse::HttpResponseCode statusCode;
          if (trx.synchronous()) {
            statusCode = rest::HttpResponse::CREATED;
          }
          else {
            statusCode = rest::HttpResponse::ACCEPTED;
          }

          generate20x(statusCode, trx.resolver()->getCollectionName(cid), (TRI_voc_key_t) TRI_EXTRACT_MARKER_KEY(&mptr), mptr._rid);  // PROTECTED by trx here
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates deleted message
////////////////////////////////////////////////////////////////////////////////

        void generateDeleted (triagens::arango::SingleCollectionWriteTransaction<1>& trx,
                              TRI_voc_cid_t cid,
                              TRI_voc_key_t key,
                              TRI_voc_rid_t rid) {

          rest::HttpResponse::HttpResponseCode statusCode;
          if (trx.synchronous()) {
            statusCode = rest::HttpResponse::OK;
          }
          else {
            statusCode = rest::HttpResponse::ACCEPTED;
          }

          generate20x(statusCode, trx.resolver()->getCollectionName(cid), key, rid);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates document not found error message, read transaction
////////////////////////////////////////////////////////////////////////////////

        void generateDocumentNotFound (triagens::arango::SingleCollectionReadOnlyTransaction& trx,
                                       TRI_voc_cid_t cid,
                                       TRI_voc_key_t key) {
          generateDocumentNotFound(trx.resolver()->getCollectionName(cid), key);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates document not found error message, write transaction
////////////////////////////////////////////////////////////////////////////////

        void generateDocumentNotFound (triagens::arango::SingleCollectionWriteTransaction<1>& trx,
                                       TRI_voc_cid_t cid,
                                       TRI_voc_key_t key) {
          generateDocumentNotFound(trx.resolver()->getCollectionName(cid), key);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates document not found error message, no transaction info
////////////////////////////////////////////////////////////////////////////////

        void generateDocumentNotFound (std::string const& collectionName,
                                       TRI_voc_key_t key) {
          generateError(rest::HttpResponse::NOT_FOUND,
                        TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                        "document " + DOCUMENT_PATH + "/" +
                        DocumentHelper::assembleDocumentId(collectionName, key) + " not found");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not implemented
////////////////////////////////////////////////////////////////////////////////

        void generateNotImplemented (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates forbidden
////////////////////////////////////////////////////////////////////////////////

        void generateForbidden ();

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed, for a read-transaction
////////////////////////////////////////////////////////////////////////////////

        void generatePreconditionFailed (SingleCollectionReadOnlyTransaction& trx,
                                         TRI_voc_cid_t cid,
                                         TRI_doc_mptr_copy_t const& mptr,
                                         TRI_voc_rid_t rid) {
          return generatePreconditionFailed(trx.resolver()->getCollectionName(cid), (TRI_voc_key_t) TRI_EXTRACT_MARKER_KEY(&mptr), rid);  // PROTECTED by RUNTIME
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed, for a write-transaction
////////////////////////////////////////////////////////////////////////////////

        void generatePreconditionFailed (SingleCollectionWriteTransaction<1>& trx,
                                         TRI_voc_cid_t cid,
                                         TRI_doc_mptr_copy_t const& mptr,
                                         TRI_voc_rid_t rid) {
          return generatePreconditionFailed(trx.resolver()->getCollectionName(cid), (TRI_voc_key_t) TRI_EXTRACT_MARKER_KEY(&mptr), rid);  // PROTECTED by RUNTIME
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed, without transaction info
////////////////////////////////////////////////////////////////////////////////

        void generatePreconditionFailed (std::string const&,
                                         TRI_voc_key_t key,
                                         TRI_voc_rid_t rid);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not modified
////////////////////////////////////////////////////////////////////////////////

        void generateNotModified (TRI_voc_rid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates first entry from a result set
////////////////////////////////////////////////////////////////////////////////

        void generateDocument (SingleCollectionReadOnlyTransaction& trx,
                               TRI_voc_cid_t,
                               TRI_doc_mptr_copy_t const&,
                               TRI_shaper_t*,
                               bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate an error message for a transaction error
////////////////////////////////////////////////////////////////////////////////

        void generateTransactionError (std::string const&,
                                       int,
                                       TRI_voc_key_t = 0,
                                       TRI_voc_rid_t = 0);

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision
///
/// @note @FA{header} must be lowercase.
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_rid_t extractRevision (char const*,
                                       char const*,
                                       bool&);

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the update policy
////////////////////////////////////////////////////////////////////////////////

        TRI_doc_update_policy_e extractUpdatePolicy () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the waitForSync value
////////////////////////////////////////////////////////////////////////////////

        bool extractWaitForSync () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the body
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* parseJsonBody ();

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a string attribute from a JSON array
///
/// if the attribute is not there or not a string, this returns 0
////////////////////////////////////////////////////////////////////////////////

        char const* extractJsonStringValue (TRI_json_t const*,
                                            char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a document handle, on a cluster this will parse the
/// collection name as a cluster-wide collection name and return a
/// cluster-wide collection ID in `cid`.
////////////////////////////////////////////////////////////////////////////////

        int parseDocumentId (triagens::arango::CollectionNameResolver const*,
                             std::string const&,
                             TRI_voc_cid_t&,
                             TRI_voc_key_t&);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief request context
////////////////////////////////////////////////////////////////////////////////

        VocbaseContext* _context;

////////////////////////////////////////////////////////////////////////////////
/// @brief the vocbase
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* _vocbase;

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool isDirect () {
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        std::string const& queue () const;

    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

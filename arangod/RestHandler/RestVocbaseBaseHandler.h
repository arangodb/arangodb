////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_REST_HANDLER_REST_VOCBASE_BASE_HANDLER_H
#define TRIAGENS_REST_HANDLER_REST_VOCBASE_BASE_HANDLER_H 1

#include "Admin/RestBaseHandler.h"

#include "BasicsC/json.h"
#include "BasicsC/json-utilities.h"

#include "Logger/Logger.h"
#include "Rest/HttpResponse.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/RestTransactionContext.h"
#include "Utils/SingleCollectionReadOnlyTransaction.h"
#include "Utils/SingleCollectionWriteTransaction.h"
#include "Utils/StandaloneTransaction.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

extern "C" {
  struct TRI_json_s;
  struct TRI_primary_collection_s;
  struct TRI_vocbase_col_s;
  struct TRI_vocbase_s;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      class RestVocbaseBaseHandler
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

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

        static string BATCH_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief document path
////////////////////////////////////////////////////////////////////////////////

        static string DOCUMENT_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief document import path
////////////////////////////////////////////////////////////////////////////////

        static string DOCUMENT_IMPORT_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge path
////////////////////////////////////////////////////////////////////////////////

        static string EDGE_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief replication path
////////////////////////////////////////////////////////////////////////////////

        static string REPLICATION_PATH;

////////////////////////////////////////////////////////////////////////////////
/// @brief upload path
////////////////////////////////////////////////////////////////////////////////

        static string UPLOAD_PATH;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestVocbaseBaseHandler (rest::HttpRequest*); 

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~RestVocbaseBaseHandler ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection needs to be created on the fly
///
/// this method will check the "createCollection" attribute of the request. if
/// it is set to true, it will verify that the named collection actually exists.
/// if the collection does not yet exist, it will create it on the fly.
/// if the "createCollection" attribute is not set or set to false, nothing will
/// happen, and the collection name will not be checked
////////////////////////////////////////////////////////////////////////////////

        bool checkCreateCollection (const string&,
                                    const TRI_col_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a HTTP 201 or 202 response
////////////////////////////////////////////////////////////////////////////////

        void generate20x (const rest::HttpResponse::HttpResponseCode,
                          const string&,
                          const TRI_voc_key_t,
                          const TRI_voc_rid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates ok message without content
////////////////////////////////////////////////////////////////////////////////

        void generateOk () {
          _response = createResponse(rest::HttpResponse::NO_CONTENT);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates ok message with no body but with certain status code
////////////////////////////////////////////////////////////////////////////////

        void generateOk (const rest::HttpResponse::HttpResponseCode code) {
          _response = createResponse(code);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates created message
////////////////////////////////////////////////////////////////////////////////

        void generateCreated (const TRI_voc_cid_t cid,
                              TRI_voc_key_t key,
                              TRI_voc_rid_t rid) {
          generate20x(rest::HttpResponse::CREATED, _resolver.getCollectionName(cid), key, rid);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates accepted message
////////////////////////////////////////////////////////////////////////////////

        void generateAccepted (const TRI_voc_cid_t cid,
                               const TRI_voc_key_t key,
                               const TRI_voc_rid_t rid) {
          generate20x(rest::HttpResponse::ACCEPTED, _resolver.getCollectionName(cid), key, rid);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates deleted message
////////////////////////////////////////////////////////////////////////////////

        void generateDeleted (const TRI_voc_cid_t cid,
                              const TRI_voc_key_t key,
                              const TRI_voc_rid_t rid) {
          generate20x(rest::HttpResponse::OK, _resolver.getCollectionName(cid), key, rid);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates updated message
////////////////////////////////////////////////////////////////////////////////

        void generateUpdated (const TRI_voc_cid_t cid,
                              const TRI_voc_key_t key,
                              const TRI_voc_rid_t rid) {
          generate20x(rest::HttpResponse::OK, _resolver.getCollectionName(cid), key, rid);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief generates document not found error message
////////////////////////////////////////////////////////////////////////////////

        void generateDocumentNotFound (const TRI_voc_cid_t,
                                       TRI_voc_key_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not implemented
////////////////////////////////////////////////////////////////////////////////

        void generateNotImplemented (const string&);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates forbidden
////////////////////////////////////////////////////////////////////////////////

        void generateForbidden ();

////////////////////////////////////////////////////////////////////////////////
/// @brief generates precondition failed
////////////////////////////////////////////////////////////////////////////////

        void generatePreconditionFailed (const TRI_voc_cid_t,
                                         TRI_voc_key_t,
                                         TRI_voc_rid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates not modified
////////////////////////////////////////////////////////////////////////////////

        void generateNotModified (const TRI_voc_rid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief generates first entry from a result set
////////////////////////////////////////////////////////////////////////////////

        void generateDocument (const TRI_voc_cid_t,
                               TRI_doc_mptr_t const*,
                               TRI_shaper_t*,
                               const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate an error message for a transaction error
////////////////////////////////////////////////////////////////////////////////

        void generateTransactionError (const string&,
                                       const int,
                                       TRI_voc_key_t = 0,
                                       TRI_voc_rid_t = 0);


////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision
///
/// @note @FA{header} must be lowercase.
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_rid_t extractRevision (char const*, 
                                       char const*);

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

        struct TRI_json_s* parseJsonBody ();

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a string attribute from a JSON array
///
/// if the attribute is not there or not a string, this returns 0
////////////////////////////////////////////////////////////////////////////////

        char const* extractJsonStringValue (const TRI_json_t* const,
                                            char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a document handle
////////////////////////////////////////////////////////////////////////////////

        int parseDocumentId (string const&, 
                             TRI_voc_cid_t&, 
                             TRI_voc_key_t&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief request context
////////////////////////////////////////////////////////////////////////////////

        VocbaseContext* _context;

////////////////////////////////////////////////////////////////////////////////
/// @brief the vocbase
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief utility object to look up names for collection ids and vice versa
////////////////////////////////////////////////////////////////////////////////

        triagens::arango::CollectionNameResolver _resolver;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool isDirect ();
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

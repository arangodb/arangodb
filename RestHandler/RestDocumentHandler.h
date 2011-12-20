////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_AVOCADO_DB_REST_HANDLER_REST_DOCUMENT_HANDLER_H
#define TRIAGENS_AVOCADO_DB_REST_HANDLER_REST_DOCUMENT_HANDLER_H 1

#include "RestHandler/RestVocbaseBaseHandler.h"

////////////////////////////////////////////////////////////////////////////////
/// @page RestDocumentTOC
///
/// <ol>
///   <li>@ref RestDocumentCreate "POST /_document"</li>
///   <li>@ref RestDocumentRead "GET /_document"</li>
///   <li>@ref RestDocumentUpdate "PUT /_document"</li>
///   <li>@ref RestDocumentDelete "DELETE /_document"</li>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page RestDocument REST Interface for Documents
///
/// The basic operations (create, read, update, delete) for documents are mapped
/// to the standard HTTP methods (POST, GET, PUT, DELETE). An identifier for the
/// revision is returned in the "ETag" field. If you modify a document, you can
/// use the "ETag" field to detect conflicts.
///
/// <hr>
/// @copydoc RestDocumentTOC
/// <hr>
///
/// @anchor RestDocumentCreate
/// @copydetails triagens::avocado::RestDocumentHandler::createDocument
///
/// @anchor RestDocumentRead
/// @copydetails triagens::avocado::RestDocumentHandler::readDocument
///
/// @anchor RestDocumentUpdate
/// @copydetails triagens::avocado::RestDocumentHandler::updateDocument
///
/// @anchor RestDocumentDelete
/// @copydetails triagens::avocado::RestDocumentHandler::deleteDocument
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               RestDocumentHandler
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace avocado {

////////////////////////////////////////////////////////////////////////////////
/// @brief document request handler
////////////////////////////////////////////////////////////////////////////////

    class RestDocumentHandler : public RestVocbaseBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestDocumentHandler (rest::HttpRequest* request, struct TRI_vocbase_s* vocbase);

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        status_e execute ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

    private:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document
////////////////////////////////////////////////////////////////////////////////

      bool createDocument ();

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a document
////////////////////////////////////////////////////////////////////////////////

      bool readDocument ();

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
////////////////////////////////////////////////////////////////////////////////

      bool updateDocument ();

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
////////////////////////////////////////////////////////////////////////////////

      bool deleteDocument ();
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

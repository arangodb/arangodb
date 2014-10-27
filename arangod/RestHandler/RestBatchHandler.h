////////////////////////////////////////////////////////////////////////////////
/// @brief batch request handler
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_HANDLER_REST_BATCH_HANDLER_H
#define ARANGODB_REST_HANDLER_REST_BATCH_HANDLER_H 1

#include "Basics/Common.h"

#include "RestHandler/RestVocbaseBaseHandler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                            class RestBatchHandler
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief container for complete multipart message
////////////////////////////////////////////////////////////////////////////////

    struct MultipartMessage {
      MultipartMessage (const char* boundary,
                        const size_t boundaryLength,
                        const char* messageStart,
                        const char* messageEnd)
      : boundary(boundary),
        boundaryLength(boundaryLength),
        messageStart(messageStart),
        messageEnd(messageEnd) {
      };

      const char* boundary;
      const size_t boundaryLength;
      const char* messageStart;
      const char* messageEnd;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief container for search data within multipart message
////////////////////////////////////////////////////////////////////////////////

    struct SearchHelper {
      MultipartMessage* message;
      char* searchStart;
      char* foundStart;
      size_t foundLength;
      char* contentId;
      size_t contentIdLength;
      bool containsMore;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief batch request handler
////////////////////////////////////////////////////////////////////////////////

    class RestBatchHandler : public RestVocbaseBaseHandler {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        RestBatchHandler (rest::HttpRequest*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~RestBatchHandler ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        Handler::status_t execute();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the body of a multipart message
////////////////////////////////////////////////////////////////////////////////

        bool getBoundaryBody (std::string*);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the HTTP header of a multipart message
////////////////////////////////////////////////////////////////////////////////

        bool getBoundaryHeader (std::string*);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary of a multipart message
////////////////////////////////////////////////////////////////////////////////

        bool getBoundary (std::string*);

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the next part from a multipart message
////////////////////////////////////////////////////////////////////////////////

        bool extractPart (SearchHelper*);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief part content type
////////////////////////////////////////////////////////////////////////////////

        const std::string _partContentType;

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

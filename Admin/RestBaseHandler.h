////////////////////////////////////////////////////////////////////////////////
/// @brief default handler for error handling and json in-/output
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_ADMIN_REST_BASE_HANDLER_H
#define TRIAGENS_ADMIN_REST_BASE_HANDLER_H 1

#include <Rest/HttpHandler.h>

#include <Rest/InputParser.h>
#include <Rest/HttpResponse.h>

namespace triagens {
  namespace basics {
    class VariantObject;
  }

  namespace admin {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief default handler for error handling and json in-/output
    ////////////////////////////////////////////////////////////////////////////////

    class RestBaseHandler : public rest::HttpHandler {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        RestBaseHandler (rest::HttpRequest* request);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief generates a result
        ////////////////////////////////////////////////////////////////////////////////

        virtual void generateResult (basics::VariantObject*);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief generates an error
        ////////////////////////////////////////////////////////////////////////////////

        virtual void generateError (rest::HttpResponse::HttpResponseCode, string const& details);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief parses a request body in json
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool parseBody (rest::InputParser::ObjectDescription&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void handleError (basics::TriagensError const&);
    };
  }
}

#endif

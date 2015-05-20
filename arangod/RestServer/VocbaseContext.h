////////////////////////////////////////////////////////////////////////////////
/// @brief vocbase context
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_SERVER_VOCBASE_CONTEXT_H
#define ARANGODB_REST_SERVER_VOCBASE_CONTEXT_H 1

#include "Basics/Common.h"

#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Rest/RequestContext.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_server_s;
struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                              class VocbaseContext
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB VocbaseContext
////////////////////////////////////////////////////////////////////////////////

    class VocbaseContext : public triagens::rest::RequestContext {

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a sid
////////////////////////////////////////////////////////////////////////////////

        static void createSid (std::string const& database,
                               std::string const& sid, 
                               std::string const& username);

////////////////////////////////////////////////////////////////////////////////
/// @brief clears all sid entries for a database
////////////////////////////////////////////////////////////////////////////////

        static void clearSid (std::string const& database);

////////////////////////////////////////////////////////////////////////////////
/// @brief clears a sid
////////////////////////////////////////////////////////////////////////////////

        static void clearSid (std::string const& database,
                              std::string const& sid);

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the last access time
////////////////////////////////////////////////////////////////////////////////

        static double accessSid (std::string const& database,
                                 std::string const& sid);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        VocbaseContext (rest::HttpRequest*,
                        struct TRI_server_s*,
                        struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~VocbaseContext ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief get vocbase of context
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* getVocbase () const {
          return _vocbase;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to use special cluster authentication
////////////////////////////////////////////////////////////////////////////////

        bool useClusterAuthentication () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return authentication realm
////////////////////////////////////////////////////////////////////////////////

        char const* getRealm () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the authentication
////////////////////////////////////////////////////////////////////////////////

        rest::HttpResponse::HttpResponseCode authenticate ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief time-to-live for aardvark server sessions
////////////////////////////////////////////////////////////////////////////////

        static double ServerSessionTtl;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the server
////////////////////////////////////////////////////////////////////////////////

        struct TRI_server_s* TRI_UNUSED _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief the vocbase
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* _vocbase;

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

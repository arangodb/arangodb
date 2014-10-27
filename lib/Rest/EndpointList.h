////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint list
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_ENDPOINT_LIST_H
#define ARANGODB_REST_ENDPOINT_LIST_H 1

#include "Basics/Common.h"

#include "Rest/Endpoint.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                      EndpointList
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

    class EndpointList {

      public:

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an endpoint list
////////////////////////////////////////////////////////////////////////////////

        EndpointList ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an endpoint list
////////////////////////////////////////////////////////////////////////////////

        ~EndpointList ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether the list is empty
////////////////////////////////////////////////////////////////////////////////

        bool empty () const {
          return _endpoints.empty();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a new endpoint
////////////////////////////////////////////////////////////////////////////////

        bool add (const std::string&,
                  const std::vector<std::string>&,
                  int,
                  bool,
                  Endpoint** = 0);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a specific endpoint
////////////////////////////////////////////////////////////////////////////////

        bool remove (const std::string&,
                     Endpoint**);

////////////////////////////////////////////////////////////////////////////////
/// @brief return all databases for an endpoint
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> getMapping (const std::string&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return all endpoints
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, std::vector<std::string> > getAll () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return all endpoints with a certain prefix
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, Endpoint*> getByPrefix (const std::string&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return all endpoints with a certain encryption type
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, Endpoint*> getByPrefix (const Endpoint::EncryptionType) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return if there is an endpoint with a certain encryption type
////////////////////////////////////////////////////////////////////////////////

        bool has (const Endpoint::EncryptionType) const;

//////////////////////////////////////////////////////////////////////////////
/// @brief dump all endpoints used
////////////////////////////////////////////////////////////////////////////////

        void dump () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return an encryption name
////////////////////////////////////////////////////////////////////////////////

        static std::string getEncryptionName (const Endpoint::EncryptionType);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief list of endpoints
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, std::pair<Endpoint*, std::vector<std::string> > > _endpoints;

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

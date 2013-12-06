////////////////////////////////////////////////////////////////////////////////
/// @brief communication with agency node(s)
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Max Neunhoeffer
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_SHARDING_AGENCY_COMM_H
#define TRIAGENS_SHARDING_AGENCY_COMM_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Rest/HttpRequest.h"

#ifdef __cplusplus
extern "C" {
#endif


namespace triagens {
  namespace rest {
    class Endpoint;
  }

  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                  AgencyCommResult
// -----------------------------------------------------------------------------

    struct AgencyCommResult {
      AgencyCommResult ();

      ~AgencyCommResult ();

      int _statusCode;

      std::map<std::string, std::string> _values;
      
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

    class AgencyComm {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a communication channel
////////////////////////////////////////////////////////////////////////////////

        AgencyComm ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a communication channel
////////////////////////////////////////////////////////////////////////////////

        ~AgencyComm ();

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an endpoint to the agents list
////////////////////////////////////////////////////////////////////////////////

        static bool addEndpoint (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an endpoint from the agents list
////////////////////////////////////////////////////////////////////////////////

        static bool removeEndpoint (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the global prefix for all operations
////////////////////////////////////////////////////////////////////////////////
        
        static void setPrefix (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a timestamp
////////////////////////////////////////////////////////////////////////////////

        static std::string generateStamp ();

// -----------------------------------------------------------------------------
// --SECTION--                                            private static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a local copy of endpoints, to be used without the global lock
////////////////////////////////////////////////////////////////////////////////

        static const std::set<std::string> getEndpoints ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief establishes the communication channels
////////////////////////////////////////////////////////////////////////////////

        int connect ();

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnects all communication channels
////////////////////////////////////////////////////////////////////////////////
        
        int disconnect ();

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the back end
////////////////////////////////////////////////////////////////////////////////

        bool setValue (std::string const& key, 
                       std::string const& value);

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or multiple values from the back end
////////////////////////////////////////////////////////////////////////////////
        
        AgencyCommResult getValues (std::string const& key, 
                                    bool recursive);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes one or multiple values from the back end
////////////////////////////////////////////////////////////////////////////////
        
        int removeValues (std::string const& key,  
                          bool recursive);

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the back end
////////////////////////////////////////////////////////////////////////////////
        
        int casValue (std::string const& key, 
                      std::string const& oldValue, 
                      std::string const& newValue);

////////////////////////////////////////////////////////////////////////////////
/// @brief blocks on a change of a single value in the back end
////////////////////////////////////////////////////////////////////////////////
        
        AgencyCommResult watchValues (std::string const& key, 
                                      double timeout);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
      
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a URL
////////////////////////////////////////////////////////////////////////////////

        std::string buildUrl (std::string const&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to the URL
////////////////////////////////////////////////////////////////////////////////
    
        bool send (triagens::rest::Endpoint*,
                       triagens::rest::HttpRequest::HttpRequestType,
                       std::string const&, 
                       std::string const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the endpoints the local instance uses
////////////////////////////////////////////////////////////////////////////////

        std::set<triagens::rest::Endpoint*> _endpoints;

////////////////////////////////////////////////////////////////////////////////
/// @brief connect timeout
////////////////////////////////////////////////////////////////////////////////

        double _connectTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief request timeout
////////////////////////////////////////////////////////////////////////////////

        double _requestTimeout;

// -----------------------------------------------------------------------------
// --SECTION--                                          private static variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the global prefix
////////////////////////////////////////////////////////////////////////////////

        static std::string _globalPrefix;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoints lock
////////////////////////////////////////////////////////////////////////////////

        static triagens::basics::ReadWriteLock _globalLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief all endpoints
////////////////////////////////////////////////////////////////////////////////
  
        static std::set<std::string> _globalEndpoints;

    };

  }
}


#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


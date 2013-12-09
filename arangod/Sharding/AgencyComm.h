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

struct TRI_json_s;

namespace triagens {
  namespace httpclient {
    class GeneralClientConnection;
  }
  
  namespace rest {
    class Endpoint;
  }

  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                    AgencyEndpoint
// -----------------------------------------------------------------------------

    struct AgencyEndpoint {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an agency endpoint
////////////////////////////////////////////////////////////////////////////////

      AgencyEndpoint (triagens::rest::Endpoint*,
                      triagens::httpclient::GeneralClientConnection*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agency endpoint
////////////////////////////////////////////////////////////////////////////////
      
      ~AgencyEndpoint (); 

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the endpoint 
////////////////////////////////////////////////////////////////////////////////

      triagens::rest::Endpoint* _endpoint;

////////////////////////////////////////////////////////////////////////////////
/// @brief the connection
////////////////////////////////////////////////////////////////////////////////

      triagens::httpclient::GeneralClientConnection* _connection;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the endpoint is busy
////////////////////////////////////////////////////////////////////////////////

      bool _busy;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                           AgencyConnectionOptions
// -----------------------------------------------------------------------------

    struct AgencyConnectionOptions {
      double _connectTimeout;
      double _requestTimeout;
      size_t _connectRetries;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  AgencyCommResult
// -----------------------------------------------------------------------------

    struct AgencyCommResult {
      AgencyCommResult ();

      ~AgencyCommResult ();

      inline bool successful () const {
        return (_statusCode >= 200 && _statusCode <= 299);
      }

      bool processJsonNode (struct TRI_json_s const*,
                            std::map<std::string, std::string>&,
                            std::string const&,
                            bool) const;

      bool flattenJson (std::map<std::string, std::string>&,
                        std::string const&,
                        bool) const; 

      std::string _message;
      std::string _body;
      int _statusCode;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

    class AgencyComm {
      friend AgencyCommResult;

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
/// @brief cleans up all connections
////////////////////////////////////////////////////////////////////////////////

        static void cleanup (); 

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to establish a communication channel
////////////////////////////////////////////////////////////////////////////////

        static bool tryConnect ();

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnects all communication channels
////////////////////////////////////////////////////////////////////////////////
        
        static void disconnect ();

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an endpoint to the agents list
////////////////////////////////////////////////////////////////////////////////

        static bool addEndpoint (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an endpoint from the agents list
////////////////////////////////////////////////////////////////////////////////

        static bool removeEndpoint (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a stringified version of the endpoints
////////////////////////////////////////////////////////////////////////////////

        static const std::string getEndpointsString ();

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
/// @brief creates a new agency endpoint
////////////////////////////////////////////////////////////////////////////////

        static AgencyEndpoint* createAgencyEndpoint (std::string const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a value in the back end
////////////////////////////////////////////////////////////////////////////////

        bool setValue (std::string const&, 
                       std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or multiple values from the back end
////////////////////////////////////////////////////////////////////////////////
        
        AgencyCommResult getValues (std::string const&, 
                                    bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes one or multiple values from the back end
////////////////////////////////////////////////////////////////////////////////
        
        bool removeValues (std::string const&,  
                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the back end
////////////////////////////////////////////////////////////////////////////////
        
        int casValue (std::string const&, 
                      std::string const&, 
                      std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief blocks on a change of a single value in the back end
////////////////////////////////////////////////////////////////////////////////
        
        AgencyCommResult watchValue (std::string const&, 
                                     uint64_t,
                                     double);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------
      
      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief pop an endpoint from the queue
////////////////////////////////////////////////////////////////////////////////
    
        AgencyEndpoint* popEndpoint ();

////////////////////////////////////////////////////////////////////////////////
/// @brief reinsert an endpoint into the queue
////////////////////////////////////////////////////////////////////////////////
    
        bool requeueEndpoint (AgencyEndpoint*,
                              bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a URL
////////////////////////////////////////////////////////////////////////////////

        std::string buildUrl (std::string const&) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to the URL w/o body
////////////////////////////////////////////////////////////////////////////////
    
        bool send (triagens::httpclient::GeneralClientConnection*,
                   triagens::rest::HttpRequest::HttpRequestType,
                   double,
                   AgencyCommResult&,
                   std::string const&); 

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to the URL w/ body
////////////////////////////////////////////////////////////////////////////////
    
        bool send (triagens::httpclient::GeneralClientConnection*,
                   triagens::rest::HttpRequest::HttpRequestType,
                   double,
                   AgencyCommResult&,
                   std::string const&, 
                   std::string const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief connect timeout
////////////////////////////////////////////////////////////////////////////////

        double _connectTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief request timeout
////////////////////////////////////////////////////////////////////////////////

        double _requestTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief connect retries
////////////////////////////////////////////////////////////////////////////////

        size_t _connectRetries;

// -----------------------------------------------------------------------------
// --SECTION--                                          private static variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the static global URL prefix
////////////////////////////////////////////////////////////////////////////////

        static const std::string AGENCY_URL_PREFIX;

////////////////////////////////////////////////////////////////////////////////
/// @brief the (variable) global prefix
////////////////////////////////////////////////////////////////////////////////

        static std::string _globalPrefix;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoints lock
////////////////////////////////////////////////////////////////////////////////

        static triagens::basics::ReadWriteLock _globalLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief all endpoints
////////////////////////////////////////////////////////////////////////////////
  
        static std::list<AgencyEndpoint*> _globalEndpoints;

////////////////////////////////////////////////////////////////////////////////
/// @brief global connection options
////////////////////////////////////////////////////////////////////////////////

        static AgencyConnectionOptions _globalConnectionOptions;

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


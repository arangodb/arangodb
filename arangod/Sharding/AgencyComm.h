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

#ifdef __cplusplus
extern "C" {
#endif


namespace triagens {
  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                                             Agent
// -----------------------------------------------------------------------------

    struct Agent {
      Agent ();
      ~Agent ();

      std::string  _name;

      std::string  _endpoint;
    };

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

      public:

        AgencyComm ();

        ~AgencyComm ();

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the global prefix for all operations
////////////////////////////////////////////////////////////////////////////////
        
        static void setPrefix (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a timestamp
////////////////////////////////////////////////////////////////////////////////

        static std::string generateStamp ();

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
/// @brief adds an agent to the agents list
////////////////////////////////////////////////////////////////////////////////

        int addAgent (Agent);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an agent from the agents list
////////////////////////////////////////////////////////////////////////////////

        int removeAgent (Agent);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an agent from the agents list
////////////////////////////////////////////////////////////////////////////////

        int setValue (std::string const& key, 
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
// --SECTION--                                          private static variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the global prefix
////////////////////////////////////////////////////////////////////////////////

        static std::string _prefix;

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


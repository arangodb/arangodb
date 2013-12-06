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
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Sharding/AgencyComm.h"
#include "BasicsC/logging.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                             Agent
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an agent object
////////////////////////////////////////////////////////////////////////////////

Agent::Agent () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agent object
////////////////////////////////////////////////////////////////////////////////

Agent::~Agent () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  AgencyCommResult
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a communication result
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult::AgencyCommResult () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a communication result
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult::~AgencyCommResult () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        AgencyComm
// -----------------------------------------------------------------------------

std::string AgencyComm::_prefix;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an agency communication object
////////////////////////////////////////////////////////////////////////////////

AgencyComm::AgencyComm () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an agency communication object
////////////////////////////////////////////////////////////////////////////////

AgencyComm::~AgencyComm () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the global prefix for all operations
////////////////////////////////////////////////////////////////////////////////

void AgencyComm::setPrefix (std::string const& prefix) {
  _prefix = prefix;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a timestamp
////////////////////////////////////////////////////////////////////////////////

std::string AgencyComm::generateStamp () {
  time_t tt = time(0);
  struct tm tb;
  char buffer[21];

  // TODO: optimise this
  TRI_gmtime(tt, &tb);

  size_t len = ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);

  return std::string(buffer, len);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief establishes the communication channels
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::connect () {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnects all communication channels
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::disconnect () {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an agent to the agents list
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::addAgent (Agent agent) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an agent from the agents list
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::removeAgent (Agent agent) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an agent from the agents list
////////////////////////////////////////////////////////////////////////////////
        
int AgencyComm::setValue (std::string const& key, 
                          std::string const& value) {

  // LOG_INFO("SETTING VALUE: %s TO %s", key.c_str(), value.c_str());
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or multiple values from the back end
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::getValues (std::string const& key, 
                                        bool recursive) {
  AgencyCommResult result;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes one or multiple values from the back end
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::removeValues (std::string const& key, 
                              bool recursive) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares and swaps a single value in the back end
////////////////////////////////////////////////////////////////////////////////

int AgencyComm::casValue (std::string const& key,
                          std::string const& oldValue, 
                          std::string const& newValue) {
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief blocks on a change of a single value in the back end
////////////////////////////////////////////////////////////////////////////////

AgencyCommResult AgencyComm::watchValues (std::string const& key, 
                                          double timeout) {
  AgencyCommResult result;

  return result;
} 

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


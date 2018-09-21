////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "GossipCallback.h"

#include "Agency/Agent.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

GossipCallback::GossipCallback(Agent* agent, size_t version) :
  _agent(agent), _version(version) {}

bool GossipCallback::operator()(arangodb::ClusterCommResult* res) {
  
  std::string newLocation;
  
  if (res->status == CL_COMM_SENT) {
    auto const& returnCode = res->result->getHttpReturnCode();
    
    switch (returnCode) {

    case 200 : // Digest other configuration
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Got result of gossip message, code: " << returnCode 
        << " body: " << res->result->getBodyVelocyPack()->slice().toJson();
      _agent->gossip(res->result->getBodyVelocyPack(), true, _version);
      break;

    case 307 : // Add new endpoint to gossip peers
      static std::string const LOCATION = "Location";
      bool found;
      newLocation = res->result->getHeaderField(LOCATION, found);
      if (found) {
        LOG_TOPIC(DEBUG, Logger::AGENCY)
          << "Got redirect to " << newLocation << ". Adding peer to gossip peers"; 
        bool added = _agent->addGossipPeer(newLocation);
        if (added) {
          LOG_TOPIC(DEBUG, Logger::AGENCY)
            << "Added " << newLocation << " to gossip peers";
        } else {
          LOG_TOPIC(DEBUG, Logger::AGENCY)
            << "Endpoint " << newLocation << " already known";
        }
      } else {
        LOG_TOPIC(ERR, Logger::AGENCY) << "Redirect lacks 'Location' header";
      }
      break;
      
    default:
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Got error " << returnCode  << " from gossip endpoint";
      break;
      
    }
    
  }
  
  LOG_TOPIC(DEBUG, Logger::AGENCY)
    << "Got error from gossip message, status:" << res->status;
  
  return true;

}

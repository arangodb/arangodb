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

#include "AgentCallback.h"

#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"

using namespace arangodb::application_features;
using namespace arangodb::consensus;
using namespace arangodb::velocypack;

AgentCallback::AgentCallback() :
  _agent(nullptr), _last(0), _toLog(0), _startTime(0.0) {}

AgentCallback::AgentCallback(Agent* agent, std::string const& slaveID,
                             index_t last, size_t toLog)
  : _agent(agent), _last(last), _slaveID(slaveID), _toLog(toLog),
    _startTime(TRI_microtime())  {}

void AgentCallback::shutdown() { _agent = nullptr; }

bool AgentCallback::operator()(arangodb::ClusterCommResult* res) {
  if (res->status == CL_COMM_SENT) {
    if (_agent) {
      auto body = res->result->getBodyVelocyPack();
      bool success = false;
      term_t otherTerm = 0;
      try {
        success = body->slice().get("success").isTrue();
        otherTerm = body->slice().get("term").getNumber<term_t>();
      } catch (std::exception const& e) {
        LOG_TOPIC(WARN, Logger::AGENCY) 
          << "Bad callback message received: " << e.what();
        _agent->reportFailed(_slaveID, _toLog);
      }
      if (otherTerm > _agent->term()) {
        _agent->resign(otherTerm);
      } else if (!success) {
        LOG_TOPIC(DEBUG, Logger::CLUSTER)
          << "Got negative answer from follower, will retry later.";
        // This reportFailed will reset _confirmed in Agent fot this follower
        _agent->reportFailed(_slaveID, _toLog, true);
      } else {
        Slice senderTimeStamp = body->slice().get("senderTimeStamp");
        if (senderTimeStamp.isInteger()) {
          try {
            int64_t sts = senderTimeStamp.getNumber<int64_t>();
            int64_t now = std::llround(steadyClockToDouble() * 1000);
            if (now - sts > 1000) {  // a second round trip time!
              LOG_TOPIC(DEBUG, Logger::AGENCY)
                << "Round trip for appendEntriesRPC took " << now - sts
                << " milliseconds, which is way too high!";
            }
          } catch(...) {
            LOG_TOPIC(WARN, Logger::AGENCY)
              << "Exception when looking at senderTimeStamp in appendEntriesRPC"
                 " answer.";
          }
        }
          
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "AgentCallback: "
          << body->slice().toJson();
        _agent->reportIn(_slaveID, _last, _toLog);
      }
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Got good callback from AppendEntriesRPC: "
      << "comm_status(" << res->status
      << "), last(" << _last << "), follower("
      << _slaveID << "), time("
      << TRI_microtime() - _startTime << ")";
  } else {
    if (!ApplicationServer::isStopping() &&
        (_agent == nullptr || !_agent->isStopping())) {
      // Do not warn if we are already shutting down:
      LOG_TOPIC(WARN, Logger::AGENCY) 
        << "Got bad callback from AppendEntriesRPC: "
        << "comm_status(" << res->status
        << "), last(" << _last << "), follower("
        << _slaveID << "), time("
        << TRI_microtime() - _startTime << ")";
    }
    if (_agent != nullptr) {
      _agent->reportFailed(_slaveID, _toLog);
    }
  }
  return true;
}

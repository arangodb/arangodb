////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Log.h"

#include <chrono>
#include <thread>

namespace arangodb {
namespace consensus {

Log::Log() : Thread("Log") {}
Log::~Log() {}

void Log::configure(Agent* agent) {
  _agent = agent;
}

void Log::respHandler (index_t idx) {
  // Handle responses
  
}

bool Log::operator()(ClusterCommResult*) {
  
};

template<> id_t Log::log (
  std::shared_ptr<arangodb::velocypack::Builder> const& builder) {

  // Write transaction 1ST!
  if (_state.write (builder)) {
/*
    // Tell everyone else
    std::string body = builder.toString();
    arangodb::velocypack::Options opts;
    std::unique_ptr<std::map<std::string, std::string>> headerFields =
      std::make_unique<std::map<std::string, std::string> >();
    std::vector<ClusterCommResult> results(_agent->config().end_points.size());
    std::stringstream path;
    path << "/_api/agency/log?id=" << _id << "&term=" << _term;
    
    for (size_t i = 0; i < _agent->config().end_points.size(); ++i) { // Ask everyone for their vote
      if (i != _id) {
        results[i] = arangodb::ClusterComm::instance()->asyncRequest("1", 1,
        _agent->config().end_points[i], rest::HttpRequest::HTTP_REQUEST_GET,
        path.str(), std::make_shared<std::string>(body), headerFields,
        std::make_shared<Log>(),
        _timeout, true);
        LOG(WARN) << _agent->config().end_points[i];
      }
*/
    }
  
  return 0;
}


void Log::run() {
  while (true) {
    std::this_thread::sleep_for(duration_t(1.0));
  }
}
  

}}

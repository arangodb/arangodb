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

#include "StoreCallback.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

StoreCallback::StoreCallback(std::string const& path, std::string const& body)
  : _path(path) , _body(body){}

bool StoreCallback::operator()(arangodb::ClusterCommResult* res) {
  if (res->status != CL_COMM_SENT) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << res->endpoint + _path << "(" << res->status << ", " << res->errorMessage
      << "): " << _body;
  }
  return true;
}

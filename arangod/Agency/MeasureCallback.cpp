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

#include "MeasureCallback.h"
#include "Agent.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

MeasureCallback::MeasureCallback(
  Inception* inc, std::string const& peerId, uint64_t sent) :
  _inc(inc), _peerId(peerId), _sent(sent){}

bool MeasureCallback::operator()(arangodb::ClusterCommResult* res) {
  if (res->status == CL_COMM_SENT && res->result->getHttpReturnCode() == 200) {
    _inc->reportIn(_peerId, _sent);
  }
  return true;
}

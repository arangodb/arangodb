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

#ifndef ARANGOD_CONSENSUS_AGENT_CONFIGURATION_H
#define ARANGOD_CONSENSUS_AGENT_CONFIGURATION_H 1

namespace arangodb {
namespace consensus {

struct config_t {
  
  arangodb::consensus::id_t id;
  double minPing;
  double maxPing;
  std::string endpoint;
  std::vector<std::string> endpoints;
  bool notify;
  bool supervision;
  bool waitForSync;
  double supervisionFrequency;
  uint64_t compactionStepSize;
  config_t () :
    id(0),
    minPing(0.3f),
    maxPing(1.0f),
    endpoint("tcp://localhost:8529"),
    notify(false),
    supervision(false),
    waitForSync(true),
    supervisionFrequency(5.0),
    compactionStepSize(1000) {}
  
  config_t (uint32_t i, double minp, double maxp, std::string ep,
            std::vector<std::string> const& eps, bool n,
            bool s, bool w, double f, uint64_t c) :
    id(i),
    minPing(minp),
    maxPing(maxp),
    endpoint(ep),
    endpoints(eps),
    notify(n),
    supervision(s),
    waitForSync(w),
    supervisionFrequency(f),
    compactionStepSize(c) {}
  
  inline size_t size() const {return endpoints.size();}

  query_t const toBuilder () const {
    query_t ret = std::make_shared<arangodb::velocypack::Builder>();
    ret->openObject();
    ret->add("endpoints", VPackValue(VPackValueType::Array));
    for (auto const& i : endpoints)
      ret->add(VPackValue(i));
    ret->close();
    ret->add("endpoint", VPackValue(endpoint));
    ret->add("id",VPackValue(id));
    ret->add("minPing",VPackValue(minPing));
    ret->add("maxPing",VPackValue(maxPing));
    ret->add("notify peers", VPackValue(notify));
    ret->add("supervision", VPackValue(supervision));
    ret->add("supervision frequency", VPackValue(supervisionFrequency));
    ret->add("compaction step size", VPackValue(compactionStepSize));
    ret->close();
    return ret;
  }
  
};

}}

#endif 

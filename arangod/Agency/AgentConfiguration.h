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

#ifndef ARANGODB_CONSENSUS_AGENT_CONFIGURATION_H
#define ARANGODB_CONSENSUS_AGENT_CONFIGURATION_H

namespace arangodb {
namespace consensus {

struct config_t {
  
  id_t id;
  double min_ping;
  double max_ping;
  std::string end_point;
  std::vector<std::string> end_points;
  bool notify;
  bool supervision;
  bool wait_for_sync;
  double supervision_frequency;
  
  config_t () :
  id(0),
    min_ping(0.3f),
    max_ping(1.0f),
    end_point("tcp://localhost:8529"),
    notify(false),
    supervision(false),
    wait_for_sync(true),
    supervision_frequency(5.0) {}
  
  config_t (uint32_t i, double min_p, double max_p, std::string ep,
	    std::vector<std::string> const& eps, bool n,
	    bool s, bool w, double f) :
    id(i),
    min_ping(min_p),
    max_ping(max_p),
    end_point(ep),
    end_points(eps),
    notify(n),
    supervision(s),
    wait_for_sync(w),
    supervision_frequency(f){}
  
  inline size_t size() const {return end_points.size();}

  query_t const toBuilder () const {
    query_t ret = std::make_shared<arangodb::velocypack::Builder>();
    ret->openObject();
    ret->add("endpoints", VPackValue(VPackValueType::Array));
    for (auto const& i : end_points)
      ret->add(VPackValue(i));
    ret->close();
    ret->add("endpoint", VPackValue(end_point));
    ret->add("id",VPackValue(id));
    ret->add("min_ping",VPackValue(min_ping));
    ret->add("max_ping",VPackValue(max_ping));
    ret->add("notify peers", VPackValue(notify));
    ret->add("supervision", VPackValue(supervision));
    ret->add("supervision frequency", VPackValue(supervision_frequency));
    ret->close();
    return ret;
  }
  
};

}}

#endif 

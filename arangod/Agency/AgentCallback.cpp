#include "AgentCallback.h"
#include "AgencyCommon.h"
#include "Agent.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

AgentCallback::AgentCallback() : _agent(0) {}

AgentCallback::AgentCallback(Agent* agent) : _agent(agent) {}

void AgentCallback::shutdown() {
  _agent = 0;
}

#include <iostream>
bool AgentCallback::operator()(arangodb::ClusterCommResult* res) {

  std::cout << "I WAS FUCKING CALLED " << std::endl;
  
  if (res->status == CL_COMM_RECEIVED) {
    id_t agent_id;
    std::vector<index_t> idx;
    std::shared_ptr<VPackBuilder> builder = res->result->getBodyVelocyPack();
    if (builder->hasKey("agent_id")) {
      agent_id = builder->getKey("agent_id").getUInt();
    } else {
      return true;
    }
    if (builder->hasKey("indices")) {
      builder->getKey("indices");
      if (builder->getKey("indices").isArray()) {
        for (size_t i = 0; i < builder->getKey("indices").length(); ++i) {
          idx.push_back(builder->getKey("indices")[i].getUInt());
        }
      }
    }
    if(_agent) {
      _agent->reportIn (agent_id, idx.back());
    }
  }
  return true;
}
  

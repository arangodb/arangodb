#include "AgentCallbacks.h"

AgentCallbacks::AgentCallbacks() : _agent(0) {}

AgentCallbacks::AgentCallbacks(Agent* agent) : _agent(agent) {}

AgentCallbacks::shutdown() {
  _agent = 0;
}

bool AgentCallbacks::operator()(ClusterCommResult* res) {
  if (res->status == CL_COMM_RECEIVED) {
    id_t agent_id;
    std::vector<index_t> idx;
    std::shared_ptr< VPackBuilder > builder = res->result->getVelocyPack();
    if (builder->hasKey("agent_id")) {
      agent_id = getUInt("agent_id");
    }
    if (builder->hasKey("indices")) {
      Slice indices = builder.getKey("indices");
      if (indices.isArray()) {
        for (size_t i = 0; i < indices.length(); ++i) {
          idx.push_back(indices[i].getUInt());
        }
      }
    }
    if(_agent) {
      _agent->reportIn (agent_id, idx);
    }
  }
  return true;
}
  

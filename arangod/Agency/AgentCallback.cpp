#include "AgentCallback.h"
#include "AgencyCommon.h"
#include "Agent.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

AgentCallback::AgentCallback() : _agent(0) {}

AgentCallback::AgentCallback(Agent* agent) : _agent(agent) {}

void AgentCallbacbk::shutdown() {
  _agent = 0;
}

bool AgentCallback::operator()(arangodb::ClusterCommResult* res) {
  
  if (res->status == CL_COMM_RECEIVED) {
    id_t agent_id;
    std::vector<index_t> idx;
    std::shared_ptr< VPackBuilder > builder = res->result->getBodyVelocyPack();
    if (builder->hasKey("agent_id")) {
      agent_id = builder->getKey("agent_id").getUInt();
    }
    if (builder->hasKey("indices")) {
      Slice indices = builder->getKey("indices");
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
  

#include "AgentCallback.h"
#include "Agent.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

AgentCallback::AgentCallback() : _agent(0) {}

AgentCallback::AgentCallback(Agent* agent, id_t slave_id, index_t last) :
  _agent(agent), _last(last), _slave_id(slave_id) {}

void AgentCallback::shutdown() {
  _agent = 0;
}

bool AgentCallback::operator()(arangodb::ClusterCommResult* res) {

  if (res->status == CL_COMM_SENT) {
    if(_agent) {
      _agent->reportIn (_slave_id, _last);
    }
  }
  return true;
  
}
  

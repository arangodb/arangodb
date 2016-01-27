#include "Agent.h"

using namespace arangodb::consensus;

Agent::Agent () {}

Agent::Agent (AgentConfig<double> const&) {}

Agent::Agent (Agent const&) {}

Agent::~Agent () {}

Constituent::term_t Agent::term () const {
  return _constituent.term();
}

Slice const& Agent::log (const Slice& slice) {
  if (_constituent.leader())
    return _log.log(slice);
  else
    return redirect(slice);
}

Slice const& Agent::redirect (const Slice& slice) {
  return slice; //TODO
}

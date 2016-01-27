#include "Constituent.h"

using namespace arangodb::consensus;

Constituent::Constituent () :
  _term(0), _gen(std::random_device()()), _mode(FOLLOWER) {
}

Constituent::Constituent (const uint32_t n) :
  _mode(FOLLOWER), _votes(std::vector<bool>(n)) {
}

Constituent::Constituent (const constituency_t& constituency) :
  _gen(std::random_device()()), _mode(FOLLOWER) {
}

Constituent::~Constituent() {}

Constituent::duration_t Constituent::sleepFor () {
  dist_t dis(0., 1.);
  return Constituent::duration_t(dis(_gen));
}

Constituent::term_t Constituent::term() const {
  return _term;
}

void Constituent::runForLeaderShip (bool b) {
}

Constituent::state_t Constituent::state () const {
  return _state;
}

Constituent::mode_t Constituent::mode () const {
  return _mode;
}

void Constituent::gossip (const Constituent::constituency_t& constituency) {
  // Talk to all peers i have not talked to
  // Answer by sending my complete list of peers
}

const Constituent::constituency_t& Constituent::gossip () {
  return _constituency;
}

bool Constituent::leader() const {
  return _mode==LEADER;
}

void Constituent::callElection() {}





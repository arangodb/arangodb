#include "SanityCheck.h"

#include "Agent.h"

using namespace arangodb::consensus;

SanityCheck::SanityCheck() : arangodb::Thread("SanityCheck"), _agent(nullptr) {}

SanityCheck::~SanityCheck() {};

void SanityCheck::configure(Agent* agent) {
  _agent = agent;
}

void SanityCheck::wakeUp () {
}

void SanityCheck::passOut () {
}

void SanityCheck::run() {
}

void SanityCheck::beginShutdown() {
}




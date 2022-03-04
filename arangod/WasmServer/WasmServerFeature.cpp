#include "WasmServerFeature.h"
#include "Cluster/ServerState.h"
#include "ApplicationFeatures/ApplicationServer.h"

using namespace arangodb;

WasmServerFeature::WasmServerFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(true);
  // TODO: check when to start WasmServerFeature
  startsAfter<CommunicationFeaturePhase>();
  startsAfter<DatabaseFeaturePhase>();
}

void WasmServerFeature::prepare() {
  if (ServerState::instance()->isCoordinator() or
      ServerState::instance()->isDBServer()) {
    setEnabled(true);
  } else {
    setEnabled(false);
  }
}

WasmServerFeature::~WasmServerFeature() = default;

void WasmServerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions>) {}
void WasmServerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {}

void WasmServerFeature::addFunction(wasm::WasmFunction const& function) {
  _guardedFunctions.doUnderLock(
      [&function](GuardedFunctions& guardedFunctions) {
        guardedFunctions._functions.emplace(function.name, function);
      });
}

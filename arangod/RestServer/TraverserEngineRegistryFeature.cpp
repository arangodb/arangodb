////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "TraverserEngineRegistryFeature.h"
#include "Cluster/TraverserEngineRegistry.h"

using namespace arangodb::application_features;

namespace arangodb {

std::atomic<traverser::TraverserEngineRegistry*> TraverserEngineRegistryFeature::TRAVERSER_ENGINE_REGISTRY{
    nullptr};

TraverserEngineRegistryFeature::TraverserEngineRegistryFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "TraverserEngineRegistry") {
  setOptional(false);
  startsAfter("V8Phase");
}

void TraverserEngineRegistryFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
}

void TraverserEngineRegistryFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
}

void TraverserEngineRegistryFeature::prepare() {
  // create the engine registery
  _engineRegistry.reset(new traverser::TraverserEngineRegistry());
  TRAVERSER_ENGINE_REGISTRY.store(_engineRegistry.get(), std::memory_order_release);
}
void TraverserEngineRegistryFeature::start() {}

void TraverserEngineRegistryFeature::unprepare() {
  TRAVERSER_ENGINE_REGISTRY.store(nullptr, std::memory_order_release);
}

}  // namespace arangodb

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

#ifndef APPLICATION_FEATURES_TRAVERSER_ENGINE_REGISTRY_FEATURE_H
#define APPLICATION_FEATURES_TRAVERSER_ENGINE_REGISTRY_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

namespace traverser {
class TraverserEngineRegistry;
}

class TraverserEngineRegistryFeature final
    : public application_features::ApplicationFeature {
 public:
      
  static traverser::TraverserEngineRegistry* registry() {
    return TRAVERSER_ENGINE_REGISTRY.load(std::memory_order_acquire);
  }

  explicit TraverserEngineRegistryFeature(
      application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

  traverser::TraverserEngineRegistry* engineRegistry() const {
    return _engineRegistry.get();
  }

 private:
  std::unique_ptr<traverser::TraverserEngineRegistry> _engineRegistry;
  static std::atomic<traverser::TraverserEngineRegistry*> TRAVERSER_ENGINE_REGISTRY;
};

}  // namespace arangodb

#endif

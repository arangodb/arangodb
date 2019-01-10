////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_SERVER_TTL_FEATURE_H
#define ARANGOD_REST_SERVER_TTL_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include <chrono>

namespace arangodb {

class TtlThread;

class TtlFeature final : public application_features::ApplicationFeature {
 public:
  explicit TtlFeature(application_features::ApplicationServer& server);
  ~TtlFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void beginShutdown() override final;
  void start() override final;
  void stop() override final;

 private:
  void shutdownThread() noexcept;

 private:
  uint64_t _frequency;
  uint64_t _maxTotalRemoves;
  uint64_t _maxCollectionRemoves;

  std::shared_ptr<TtlThread> _thread;
};

}  // namespace arangodb

#endif

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_DISPATCHER_DISPATCHER_FEATURE_H
#define ARANGOD_DISPATCHER_DISPATCHER_FEATURE_H 1

#include "Basics/Common.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationServer/ApplicationFeature.h"

namespace arangodb {
namespace rest {
class Dispatcher;
}

class DispatcherFeature final
    : public application_features::ApplicationFeature {
 public:
  static rest::Dispatcher* DISPATCHER;

 public:
  explicit DispatcherFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void start() override;
  void stop() override;

 public:
  void buildStandardQueue(size_t nrThreads, size_t maxSize);
  void buildAQLQueue(size_t nrThreads, size_t maxSize);
  void setProcessorAffinity(std::vector<size_t> const& cores);

 private:
  void buildDispatcher();
  void buildStandardQueue();
  void buildAqlQueue();

 private:
  rest::Dispatcher* _dispatcher;
  uint64_t _nrStandardThreads;
  uint64_t _nrAqlThreads;
  uint64_t _queueSize;
  bool _startAqlQueue;
};
}

#endif

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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_QUERY_REGISTRY_FEATUREx_H
#define APPLICATION_FEATURES_QUERY_REGISTRY_FEATUREx_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace aql {
class QueryRegistry;
}

class QueryRegistryFeature final : public application_features::ApplicationFeature {
 public:
  static aql::QueryRegistry* QUERY_REGISTRY;

 public:
  explicit QueryRegistryFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

 private:
  bool _queryTracking;
  double _slowThreshold;
  std::string _queryCacheMode;
  uint64_t _queryCacheEntries;

 public:
  aql::QueryRegistry* queryRegistry() const { return _queryRegistry.get(); }

 private:
  std::unique_ptr<aql::QueryRegistry> _queryRegistry;
};
}

#endif

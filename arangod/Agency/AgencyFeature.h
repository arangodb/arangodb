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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AGENCY_APPLICATION_AGENCY_H
#define ARANGOD_AGENCY_APPLICATION_AGENCY_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace consensus {
class Agent;
}

class AgencyFeature : virtual public application_features::ApplicationFeature {
 public:
  explicit AgencyFeature(application_features::ApplicationServer* server);
  ~AgencyFeature();

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

 private:
  uint64_t _size;  // agency size (default: 5)
  uint32_t _agentId;
  double _minElectionTimeout;                 // min election timeout
  double _maxElectionTimeout;                 // max election timeout
  std::vector<std::string> _agencyEndpoints;  // agency adresses
  bool _notify;  // interval between retry to slaves
  bool _supervision;
  bool _waitForSync;
  double _supervisionFrequency;
  uint64_t _compactionStepSize;

 public:
  consensus::Agent* agent() const { return _agent.get(); }

 private:
  std::unique_ptr<consensus::Agent> _agent;
};
}

#endif

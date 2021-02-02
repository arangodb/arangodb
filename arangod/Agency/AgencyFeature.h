////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AGENCY_AGENCY_FEATURE_H
#define ARANGOD_AGENCY_AGENCY_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

namespace consensus {
class Agent;
}

class AgencyFeature : public application_features::ApplicationFeature {
 public:
  explicit AgencyFeature(application_features::ApplicationServer& server);
  ~AgencyFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  bool activated() const noexcept { return _activated; }

  consensus::Agent* agent() const;

 private:
  bool _activated;
  uint64_t _size;  // agency size (default: 5)
  uint64_t _poolSize;
  double _minElectionTimeout;  // min election timeout
  double _maxElectionTimeout;  // max election timeout
  bool _supervision;
  bool _supervisionTouched;
  bool _waitForSync;
  double _supervisionFrequency;
  uint64_t _compactionStepSize;
  uint64_t _compactionKeepSize;
  uint64_t _maxAppendSize;
  double _supervisionGracePeriod;
  double _supervisionOkThreshold;
  std::string _agencyMyAddress;
  std::vector<std::string> _agencyEndpoints;
  bool _cmdLineTimings;
  std::string _recoveryId;

  std::unique_ptr<consensus::Agent> _agent;
};

}  // namespace arangodb

#endif

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_MAINTENANCE_FEATURE
#define ARANGOD_CLUSTER_MAINTENANCE_FEATURE 1

#include "Basics/Common.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

class MaintenanceFeature : public application_features::ApplicationFeature {
 public:
  explicit MaintenanceFeature(application_features::ApplicationServer*);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;

 public:
  static int32_t maintenanceThreadsMax;
};

}

#endif

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

#ifndef ARANGODB_APPLICATION_FEATURES_PRIVILEGE_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_PRIVILEGE_FEATURE_H 1

#include <sys/types.h>
#include <memory>
#include <string>

#include "Basics/operating-system.h"

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace options {
class ProgramOptions;
}

class PrivilegeFeature final : public application_features::ApplicationFeature {
 public:
  explicit PrivilegeFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;

  std::string _uid;
  std::string _gid;

  void dropPrivilegesPermanently();

 private:
  void extractPrivileges();

  TRI_uid_t _numericUid;
  TRI_gid_t _numericGid;
};

}  // namespace arangodb

#endif

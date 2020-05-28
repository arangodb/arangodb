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

#ifndef ARANGODB_APPLICATION_FEATURES_CONFIG_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_CONFIG_FEATURE_H 1

#include <memory>
#include <string>
#include <vector>

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace options {
class ProgramOptions;
}

class ConfigFeature final : public application_features::ApplicationFeature {
 public:
  ConfigFeature(application_features::ApplicationServer& server, std::string const& progname,
                std::string const& configFilename = "");

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void loadOptions(std::shared_ptr<options::ProgramOptions>,
                   char const* binaryPath) override final;

 private:
  std::string _file;
  std::vector<std::string> _defines;
  bool _checkConfiguration;

  void loadConfigFile(std::shared_ptr<options::ProgramOptions>,
                      std::string const& progname, char const* binaryPath);

  std::string _progname;
};

}  // namespace arangodb

#endif

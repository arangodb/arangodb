////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "ApplicationFeatures/ApplicationServer.h"
#include "VPack/ArangoVPackOptionProviders.h"

#include <memory>
#include <string>

namespace arangodb {
namespace options {
class ProgramOptions;
}

class ArangoVPackServer final : public application_features::ApplicationServer {
 public:
  ArangoVPackServer(std::shared_ptr<options::ProgramOptions> options,
                    char const* binaryPath, std::string binaryName, int* ret);

  void addFeatures();

 protected:
  void collectOptions() final;
  void validateOptions() final;
  void addFeaturesWithOptionProvider() final;

 private:
  std::shared_ptr<options::ProgramOptions> _programOptions;
  std::string _binaryName;
  int* _ret;
  ArangoVPackOptionProviders _optionProviders;
};

}  // namespace arangodb

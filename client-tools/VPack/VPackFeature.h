////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VPack/arangovpack.h"
#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/VelocyPackHelper.h"

namespace arangodb {

class VPackFeature final : public ArangoVPackFeature {
 public:
  static constexpr std::string_view name() noexcept { return "VPack"; }

  VPackFeature(Server& server, int* result);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void start() override;

 private:
  int* _result;
  std::string _inputFile;
  std::string _outputFile;
  std::string _inputType;
  std::string _outputType;
  bool _failOnNonJson;
};

}  // namespace arangodb

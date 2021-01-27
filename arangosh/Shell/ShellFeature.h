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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_SHELL_SHELL_FEATURE_H
#define ARANGODB_SHELL_SHELL_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class ShellFeature final : public application_features::ApplicationFeature {
 public:
  ShellFeature(application_features::ApplicationServer& server, int* result);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions> options) override;
  void start() override;
  
  void setExitCode(int code) { *_result = code; }

 private:
  std::vector<std::string> _jslint;
  std::vector<std::string> _executeScripts;
  std::vector<std::string> _executeStrings;
  std::vector<std::string> _checkSyntaxFiles;
  std::vector<std::string> _unitTests;

 public:
  enum class RunMode {
    INTERACTIVE,
    EXECUTE_SCRIPT,
    EXECUTE_STRING,
    CHECK_SYNTAX,
    UNIT_TESTS,
    JSLINT
  };

 private:
  int* _result;
  RunMode _runMode;
  std::vector<std::string> _positionals;
  std::string _unitTestFilter;
  std::vector<std::string> _scriptParameters;
  bool _runMain{false};
};

}  // namespace arangodb

#endif

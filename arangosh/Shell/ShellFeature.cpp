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

#include "ShellFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "FeaturePhases/V8ShellFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "Shell/V8ShellFeature.h"

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

ShellFeature::ShellFeature(application_features::ApplicationServer& server, int* result)
    : ApplicationFeature(server, "Shell"),
      _jslint(),
      _result(result),
      _runMode(RunMode::INTERACTIVE) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter<application_features::V8ShellFeaturePhase>();
}

void ShellFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  options->addOption("--jslint", "do not start as shell, run jslint instead",
                     new VectorParameter<StringParameter>(&_jslint));

  options->addSection("javascript", "Configure the JavaScript engine");

  options->addOption("--javascript.execute", "execute JavaScript code from file",
                     new VectorParameter<StringParameter>(&_executeScripts));

  options->addOption("--javascript.execute-string",
                     "execute JavaScript code from string",
                     new VectorParameter<StringParameter>(&_executeStrings));

  options->addOption("--javascript.check-syntax",
                     "syntax check code JavaScript code from file",
                     new VectorParameter<StringParameter>(&_checkSyntaxFiles));

  options->addOption("--javascript.unit-tests",
                     "do not start as shell, run unit tests instead",
                     new VectorParameter<StringParameter>(&_unitTests));

  options->addOption("--javascript.unit-test-filter",
                     "filter testcases in suite", new StringParameter(&_unitTestFilter));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  options->addOption("--javascript.script-parameter", "script parameter",
                     new VectorParameter<StringParameter>(&_scriptParameters));

  options->addOption("--javascript.run-main", "execute function main",
                     new BooleanParameter(&_runMain));
#endif
}

void ShellFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  _positionals = options->processingResult()._positionals;

  ClientFeature& client = server().getFeature<HttpEndpointProvider, ClientFeature>();
  ConsoleFeature& console = server().getFeature<ConsoleFeature>();

  if (client.endpoint() == "none") {
    client.disable();
  }

  if (!_jslint.empty()) {
    client.disable();
  }

  size_t n = 0;

  _runMode = RunMode::INTERACTIVE;

  if (!_executeScripts.empty()) {
    console.setQuiet(true);
    _runMode = RunMode::EXECUTE_SCRIPT;
    ++n;
  }

  if (!_executeStrings.empty()) {
    console.setQuiet(true);
    _runMode = RunMode::EXECUTE_STRING;
    ++n;
  }

  if (!_checkSyntaxFiles.empty()) {
    console.setQuiet(true);
    _runMode = RunMode::CHECK_SYNTAX;
    ++n;
  }

  if (!_unitTests.empty()) {
    console.setQuiet(true);
    _runMode = RunMode::UNIT_TESTS;
    ++n;
  }

  if (!_jslint.empty()) {
    console.setQuiet(true);
    _runMode = RunMode::JSLINT;
    ++n;
  }

  if (1 < n) {
    LOG_TOPIC("80a8c", ERR, arangodb::Logger::FIXME)
        << "you cannot specify more than one type ("
        << "jslint, execute, execute-string, check-syntax, unit-tests)";
  }
}

void ShellFeature::start() {
  *_result = EXIT_SUCCESS;

  V8ShellFeature& shell = server().getFeature<V8ShellFeature>();

  bool ok = false;

  try {
    switch (_runMode) {
      case RunMode::INTERACTIVE:
        ok = (shell.runShell(_positionals) == TRI_ERROR_NO_ERROR);
        break;

      case RunMode::EXECUTE_SCRIPT:
        ok = shell.runScript(_executeScripts, _positionals, true, _scriptParameters, _runMain);
        break;

      case RunMode::EXECUTE_STRING:
        ok = shell.runString(_executeStrings, _positionals);
        break;

      case RunMode::CHECK_SYNTAX:
        ok = shell.runScript(_checkSyntaxFiles, _positionals, false, _scriptParameters, _runMain);
        break;

      case RunMode::UNIT_TESTS:
        ok = shell.runUnitTests(_unitTests, _positionals, _unitTestFilter);
        break;

      case RunMode::JSLINT:
        ok = shell.jslint(_jslint);
        break;
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC("98f7d", ERR, arangodb::Logger::FIXME) << "caught exception: " << ex.what();
    ok = false;
  } catch (...) {
    LOG_TOPIC("4a477", ERR, arangodb::Logger::FIXME) << "caught unknown exception";
    ok = false;
  }

  if (*_result == EXIT_SUCCESS && !ok) {
    *_result = EXIT_FAILURE;
  }
}

}  // namespace arangodb

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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ShellFeature.h"

#include "Basics/debugging.h"
#include "FeaturePhases/V8ShellFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "Shell/ShellConsoleFeature.h"
#include "Shell/TelemetricsHandler.h"
#include "Shell/V8ShellFeature.h"
#include <velocypack/Builder.h>

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

ShellFeature::ShellFeature(Server& server, int* result)
    : ArangoshFeature(server, *this),
      _result(result),
      _runMode(RunMode::INTERACTIVE) {
  setOptional(false);
  startsAfter<application_features::V8ShellFeaturePhase>();
}

ShellFeature::~ShellFeature() = default;

void ShellFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("javascript", "JavaScript engine");

  options->addOption("--javascript.execute",
                     "Execute the JavaScript code from the specified file.",
                     new VectorParameter<StringParameter>(&_executeScripts));

  options->addOption("--javascript.execute-string",
                     "Execute the JavaScript code from the specified string.",
                     new VectorParameter<StringParameter>(&_executeStrings));

  options->addOption(
      "--javascript.check-syntax",
      "Check the syntax of the JavaScript code from the specified file.",
      new VectorParameter<StringParameter>(&_checkSyntaxFiles));

  options->addOption("--javascript.unit-tests",
                     "Do not start as a shell, run unit tests instead.",
                     new VectorParameter<StringParameter>(&_unitTests));

  options->addOption("--javascript.unit-test-filter",
                     "Filter the test cases in the test suite.",
                     new StringParameter(&_unitTestFilter));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  options->addOption("--javascript.script-parameter", "Script parameter.",
                     new VectorParameter<StringParameter>(&_scriptParameters));
#endif
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  options->addOption(
      "--client.failure-points",
      "The failure point to set during shell startup (requires compilation "
      "with failure points support).",
      new VectorParameter<StringParameter>(&_failurePoints));
#endif
}

void ShellFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  _positionals = options->processingResult()._positionals;

  ClientFeature& client =
      server().getFeature<HttpEndpointProvider, ClientFeature>();
  ShellConsoleFeature& console = server().getFeature<ShellConsoleFeature>();

  if (client.endpoint() == "none") {
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

  if (1 < n) {
    LOG_TOPIC("80a8c", ERR, arangodb::Logger::FIXME)
        << "you cannot specify more than one type ("
        << "execute, execute-string, check-syntax, unit-tests)";
  }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  for (auto const& it : _failurePoints) {
    TRI_AddFailurePointDebugging(it);
  }
#endif
}

void ShellFeature::start() {
  *_result = EXIT_SUCCESS;

  V8ShellFeature& shell = server().getFeature<V8ShellFeature>();

  bool ok = false;
  try {
    switch (_runMode) {
      case RunMode::INTERACTIVE:
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
        startTelemetrics();
#endif
        ok = (shell.runShell(_positionals) == TRI_ERROR_NO_ERROR);
        break;

      case RunMode::EXECUTE_SCRIPT:
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
        startTelemetrics();
#endif
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
        TRI_IF_FAILURE("startTelemetricsForTest") { startTelemetrics(); }
#endif
        ok = shell.runScript(_executeScripts, _positionals, true,
                             _scriptParameters);
        break;

      case RunMode::EXECUTE_STRING:
        ok = shell.runString(_executeStrings, _positionals);
        break;

      case RunMode::CHECK_SYNTAX:
        ok = shell.runScript(_checkSyntaxFiles, _positionals, false,
                             _scriptParameters);
        break;

      case RunMode::UNIT_TESTS:
        ok = shell.runUnitTests(_unitTests, _positionals, _unitTestFilter);
        break;
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC("98f7d", ERR, arangodb::Logger::FIXME)
        << "caught exception: " << ex.what();
    ok = false;
  } catch (...) {
    LOG_TOPIC("4a477", ERR, arangodb::Logger::FIXME)
        << "caught unknown exception";
    ok = false;
  }

  if (*_result == EXIT_SUCCESS && !ok) {
    *_result = EXIT_FAILURE;
  }
}

void ShellFeature::beginShutdown() {
  if (_telemetricsHandler != nullptr) {
    _telemetricsHandler->beginShutdown();
  }
}

void ShellFeature::stop() {
  if (_telemetricsHandler != nullptr) {
    _telemetricsHandler->joinThread();
  }
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

void ShellFeature::getTelemetricsInfo(VPackBuilder& builder) {
  if (_telemetricsHandler != nullptr) {
    _telemetricsHandler->getTelemetricsInfo(builder);
  }
}
VPackBuilder ShellFeature::sendTelemetricsToEndpoint(std::string const& url) {
  if (_telemetricsHandler != nullptr) {
    return _telemetricsHandler->sendTelemetricsToEndpoint(url);
  }
  return VPackBuilder();
}
#endif

void ShellFeature::startTelemetrics() {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  _telemetricsHandler = std::make_unique<TelemetricsHandler>(
      server(), _automaticallySendTelemetricsToEndpoint);
#else
  _telemetricsHandler = std::make_unique<TelemetricsHandler>(server(), true);
#endif
  _telemetricsHandler->runTelemetrics();
}

void ShellFeature::restartTelemetrics() {
  if (_telemetricsHandler != nullptr) {
    _telemetricsHandler->beginShutdown();
    _telemetricsHandler.reset();
  }
  startTelemetrics();
}

}  // namespace arangodb

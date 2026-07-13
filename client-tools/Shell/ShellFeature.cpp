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
#include "Shell/ShellOptionsProvider.h"

#include "Basics/debugging.h"
#include "FeaturePhases/V8ShellFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"
#include "Shell/ShellConsoleFeature.h"
#include "Shell/V8ShellFeature.h"

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

ShellFeature::ShellFeature(application_features::ApplicationServer& server,
                           int* result)
    : ShellFeature(server, result, ShellFeatureOptions{}) {}

ShellFeature::ShellFeature(application_features::ApplicationServer& server,
                           int* result, ShellFeatureOptions options)
    : ApplicationFeature(server, *this),
      _options(std::move(options)),
      _result(result),
      _runMode(RunMode::INTERACTIVE) {
  setOptional(false);
  startsAfter<application_features::V8ShellFeaturePhase>();
}

ShellFeature::~ShellFeature() = default;

void ShellFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  ShellOptionsProvider provider;
  provider.declareOptions(options, _options);
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

  if (!_options.executeScripts.empty()) {
    console.setQuiet(true);
    _runMode = RunMode::EXECUTE_SCRIPT;
    ++n;
  }

  if (!_options.executeStrings.empty()) {
    console.setQuiet(true);
    _runMode = RunMode::EXECUTE_STRING;
    ++n;
  }

  if (!_options.checkSyntaxFiles.empty()) {
    console.setQuiet(true);
    _runMode = RunMode::CHECK_SYNTAX;
    ++n;
  }

  if (!_options.unitTests.empty()) {
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
  for (auto const& it : _options.failurePoints) {
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
        ok = (shell.runShell(_positionals) == TRI_ERROR_NO_ERROR);
        break;

      case RunMode::EXECUTE_SCRIPT:
        ok = shell.runScript(_options.executeScripts, _positionals, true,
                             _options.scriptParameters);
        break;

      case RunMode::EXECUTE_STRING:
        ok = shell.runString(_options.executeStrings, _positionals);
        break;

      case RunMode::CHECK_SYNTAX:
        ok = shell.runScript(_options.checkSyntaxFiles, _positionals, false,
                             _options.scriptParameters);
        break;

      case RunMode::UNIT_TESTS:
        ok = shell.runUnitTests(_options.unitTests, _positionals,
                                _options.unitTestFilter);
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

}  // namespace arangodb

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

#include "ArangoshFeature.h"

#include "ApplicationFeatures/ClientFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "Shell/V8ShellFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

ArangoshFeature::ArangoshFeature(
    application_features::ApplicationServer* server, int* result)
    : ApplicationFeature(server, "ArangoshFeature"),
      _jslint(),
      _result(result),
      _runMode(RunMode::INTERACTIVE) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("ConfigFeature");
  startsAfter("LanguageFeature");
  startsAfter("LoggerFeature");
  startsAfter("V8ShellFeature");
}

void ArangoshFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection(
      Section("", "Global configuration", "global options", false, false));

  options->addOption("--jslint", "do not start as shell, run jslint instead",
                     new VectorParameter<StringParameter>(&_jslint));

  options->addSection("javascript", "Configure the Javascript engine");

  options->addOption("--javascript.execute",
                     "execute Javascript code from file",
                     new VectorParameter<StringParameter>(&_executeScripts));

  options->addOption("--javascript.execute-string",
                     "execute Javascript code from string",
                     new VectorParameter<StringParameter>(&_executeStrings));

  options->addOption("--javascript.check-syntax",
                     "syntax check code Javascript code from file",
                     new VectorParameter<StringParameter>(&_checkSyntaxFiles));

  options->addOption("--javascript.unit-tests",
                     "do not start as shell, run unit tests instead",
                     new VectorParameter<StringParameter>(&_unitTests));
}

void ArangoshFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::validateOptions";

  _positionals = options->processingResult()._positionals;

  ClientFeature* client =
      dynamic_cast<ClientFeature*>(server()->feature("ClientFeature"));

  ConsoleFeature* console =
      dynamic_cast<ConsoleFeature*>(server()->feature("ConsoleFeature"));

  if (client->endpoint() == "none") {
    client->disable();
  }

  if (!_jslint.empty()) {
    client->disable();
  }

  size_t n = 0;

  _runMode = RunMode::INTERACTIVE;

  if (!_executeScripts.empty()) {
    console->setQuiet(true);
    _runMode = RunMode::EXECUTE_SCRIPT;
    ++n;
  }

  if (!_executeStrings.empty()) {
    console->setQuiet(true);
    _runMode = RunMode::EXECUTE_STRING;
    ++n;
  }

  if (!_checkSyntaxFiles.empty()) {
    console->setQuiet(true);
    _runMode = RunMode::CHECK_SYNTAX;
    ++n;
  }

  if (!_unitTests.empty()) {
    console->setQuiet(true);
    _runMode = RunMode::UNIT_TESTS;
    ++n;
  }

  if (!_jslint.empty()) {
    console->setQuiet(true);
    _runMode = RunMode::JSLINT;
    ++n;
  }

  if (1 < n) {
    LOG(ERR) << "you cannot specify more than one type ("
             << "jslint, execute, execute-string, check-syntax, unit-tests)";
  }
}

void ArangoshFeature::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::start";

  *_result = EXIT_FAILURE;

  V8ShellFeature* shell =
      dynamic_cast<V8ShellFeature*>(server()->feature("V8ShellFeature"));

  bool ok = false;

  try {
    switch (_runMode) {
      case RunMode::INTERACTIVE:
        ok = shell->runShell(_positionals);
        break;

      case RunMode::EXECUTE_SCRIPT:
        ok = shell->runScript(_executeScripts, _positionals, true);
        break;

      case RunMode::EXECUTE_STRING:
        ok = shell->runString(_executeStrings, _positionals);
        break;

      case RunMode::CHECK_SYNTAX:
        ok = shell->runScript(_checkSyntaxFiles, _positionals, false);
        break;

      case RunMode::UNIT_TESTS:
        ok = shell->runUnitTests(_unitTests, _positionals);
        break;

      case RunMode::JSLINT:
        ok = shell->jslint(_jslint);
        break;
    }
  } catch (std::exception const& ex) {
    LOG(ERR) << "caught exception " << ex.what();
    ok = false;
  } catch (...) {
    LOG(ERR) << "caught unknown exception";
    ok = false;
  }

  *_result = ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

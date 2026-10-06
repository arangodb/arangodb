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
///
////////////////////////////////////////////////////////////////////////////////

#include "Shell/V8ShellOptionsProvider.h"

#include "Basics/application-exit.h"
#include "Basics/StringUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void V8ShellOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, V8ShellFeatureOptions& options) {
  opts->addSection("javascript", "JavaScript engine");

  opts->addOption(
      "--javascript.startup-directory",
      "The startup paths containing the JavaScript files.",
      new StringParameter(&options.startupDirectory),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--javascript.client-module", "The client module to use at startup.",
      new StringParameter(&options.clientModule),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--javascript.copy-directory",
      "The target directory to copy files from "
      "`--javascript.startup-directory` "
      "to (only used if `--javascript.copy-installation` is enabled).",
      new StringParameter(&options.copyDirectory));

  opts->addOption(
      "--javascript.module-directory",
      "Additional paths containing JavaScript modules.",
      new VectorParameter<StringParameter>(&options.moduleDirectories),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption("--javascript.current-module-directory",
                  "Add the current directory to the module path.",
                  new BooleanParameter(&options.currentModuleDirectory));

  opts->addOption("--javascript.copy-installation",
                  "Copy the contents of `--javascript.startup-directory`.",
                  new BooleanParameter(&options.copyInstallation));

  opts->addOption(
      "--javascript.gc-interval",
      "Request-based garbage collection interval (each n-th command).",
      new UInt64Parameter(&options.gcInterval));

  opts->addOption(
      "--javascript.execution-deadline",
      "deadline in seconds. Once reached, calls will throw. "
      "HTTP timeouts will be adjusted.",
      new UInt32Parameter(&options.executionDeadline),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
}

void V8ShellOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, V8ShellFeatureOptions& options) {
  if (options.startupDirectory.empty()) {
    LOG_TOPIC("6380f", FATAL, arangodb::Logger::FIXME)
        << "no '--javascript.startup-directory' has been supplied, giving up";
    FATAL_ERROR_EXIT();
  }

  if (!options.moduleDirectories.empty()) {
    LOG_TOPIC("90ca0", DEBUG, Logger::V8)
        << "using JavaScript modules at '"
        << basics::StringUtils::join(options.moduleDirectories, ";") << "'";
  }
}

}  // namespace arangodb

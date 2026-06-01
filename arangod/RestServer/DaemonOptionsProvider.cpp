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

#include "DaemonOptionsProvider.h"

#include <filesystem>

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void DaemonOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, DaemonFeatureOptions& opts) {
  options->addOption(
      "--daemon",
      "Start the server as a daemon (background process). Requires --pid-file "
      "to be set.",
      new BooleanParameter(&opts.daemon),
      arangodb::options::makeFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--pid-file",
      "The name of the process ID file to use if the server runs as a daemon.",
      new StringParameter(&opts.pidFile),
      arangodb::options::makeFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--working-directory", "The working directory in daemon mode.",
      new StringParameter(&opts.workingDirectory),
      arangodb::options::makeFlags(arangodb::options::Flags::Uncommon));
}

void DaemonOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> /*options*/, DaemonFeatureOptions& opts) {
  if (!opts.daemon) {
    return;
  }

  if (opts.pidFile.empty()) {
    LOG_TOPIC("9d6ba", FATAL, arangodb::Logger::FIXME)
        << "need --pid-file in --daemon mode";
    FATAL_ERROR_EXIT();
  }

  // make the pid filename absolute
  std::string absoluteFile =
      std::filesystem::absolute(std::filesystem::path(opts.pidFile)).string();

  if (!absoluteFile.empty()) {
    opts.pidFile = absoluteFile;
    LOG_TOPIC("79662", DEBUG, arangodb::Logger::FIXME)
        << "using absolute pid file '" << opts.pidFile << "'";
  } else {
    LOG_TOPIC("24de9", FATAL, arangodb::Logger::FIXME)
        << "cannot determine absolute path";
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb

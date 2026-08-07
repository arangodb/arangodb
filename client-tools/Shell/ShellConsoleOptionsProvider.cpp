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

#include "ShellConsoleOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

namespace arangodb {

using namespace arangodb::options;

void ShellConsoleOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, ShellConsoleFeatureOptions& options) {
  opts->addOption("--quiet", "Silent startup.",
                  new BooleanParameter(&options.quiet));

  opts->addSection("console", "console");

  opts->addOption(
      "--console.colors", "Enable color support.",
      new BooleanParameter(&options.colors),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  opts->addOption("--console.auto-complete", "Enable auto-completion.",
                  new BooleanParameter(&options.autoComplete));

  opts->addOption("--console.pretty-print", "Enable pretty-printing.",
                  new BooleanParameter(&options.prettyPrint));

  opts->addOption("--console.audit-file",
                  "The audit log file to save commands and results to.",
                  new StringParameter(&options.auditFile));

  opts->addOption("--console.history",
                  "Whether to load and persist command-line history.",
                  new BooleanParameter(&options.useHistory));

  opts->addOption("--console.pager", "Enable paging.",
                  new BooleanParameter(&options.pager));

  opts->addOption(
      "--console.pager-command", "The pager command.",
      new StringParameter(&options.pagerCommand),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--console.prompt",
      "The prompt used in REPL (placeholders: %t = the current time as "
      "timestamp, %p = the duration of last command in seconds, %d = the name "
      "of the current database, %e = the current endpoint, %E = the current "
      "endpoint without the protocol, %u = the current user",
      new StringParameter(&options.prompt));
}

}  // namespace arangodb

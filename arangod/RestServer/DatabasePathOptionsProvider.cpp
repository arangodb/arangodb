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

#include "DatabasePathOptionsProvider.h"

#include "Basics/ArangoGlobalContext.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/operating-system.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void DatabasePathOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, DatabasePathFeatureOptions& opts) {
  options
      ->addOption("--database.directory", "The path to the database directory.",
                  new StringParameter(&opts.directory))
      .setLongDescription(R"(This defines the location where all data of a
server is stored.

Make sure the directory is writable by the arangod process. You should further
not use a database directory which is provided by a network filesystem such as
NFS. The reason is that networked filesystems might cause inconsistencies when
there are multiple parallel readers or writers or they lack features required by
arangod, e.g. `flock()`.)");

  options->addOption(
      "--database.required-directory-state",
      "The required state of the database directory at startup "
      "(non-existing: the database directory must not exist, existing: the"
      "database directory must exist, empty: the database directory must exist "
      "but be empty, populated: the database directory must exist and contain "
      "specific files already, any: any state is allowed)",
      new DiscreteValuesParameter<StringParameter>(
          &opts.requiredDirectoryState,
          std::unordered_set<std::string>{"any", "non-existing", "existing",
                                          "empty", "populated"}));
}

void DatabasePathOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> options, DatabasePathFeatureOptions& opts) {
  auto const& positionals = options->processingResult()._positionals;

  if (1 == positionals.size()) {
    opts.directory = positionals[0];
  } else if (1 < positionals.size()) {
    LOG_TOPIC("aeb40", FATAL, arangodb::Logger::FIXME)
        << "expected at most one database directory, got '"
        << basics::StringUtils::join(positionals, ",") << "'";
    FATAL_ERROR_EXIT();
  }

  if (opts.directory.empty()) {
    LOG_TOPIC("9aba1", FATAL, arangodb::Logger::FIXME)
        << "no database path has been supplied, giving up, please use "
           "the '--database.directory' option";
    FATAL_ERROR_EXIT();
  }

  // strip trailing separators
  opts.directory =
      basics::StringUtils::rTrim(opts.directory, TRI_DIR_SEPARATOR_STR);

  auto ctx = ArangoGlobalContext::CONTEXT;

  if (ctx == nullptr) {
    LOG_TOPIC("19066", FATAL, arangodb::Logger::FIXME)
        << "failed to get global context.";
    FATAL_ERROR_EXIT();
  }

  ctx->normalizePath(opts.directory, "database.directory", false);
}

}  // namespace arangodb

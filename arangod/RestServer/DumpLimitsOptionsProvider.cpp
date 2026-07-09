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

#include "DumpLimitsOptionsProvider.h"

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void DumpLimitsOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions>& prgOptions) {
  prgOptions->addSection("dump", "Dump limits");

  prgOptions
      ->addOption(
          "--dump.max-memory-usage",
          "Maximum memory usage (in bytes) to be used by all ongoing dumps.",
          new UInt64Parameter(&_options.memoryUsage, 1,
                              /*minimum*/ 16 * 1024 * 1024),
          makeFlags(Flags::Dynamic, Flags::DefaultNoComponents,
                    Flags::OnDBServer, Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(The approximate per-server maximum allowed memory usage value
for all ongoing dump actions combined.)");

  prgOptions
      ->addOption(
          "--dump.max-docs-per-batch",
          "Maximum number of documents per batch that can be used in a dump.",
          new UInt64Parameter(&_options.docsPerBatchUpperBound, 1,
                              /*minimum*/ _options.docsPerBatchLowerBound),
          makeFlags(Flags::Uncommon, Flags::DefaultNoComponents,
                    Flags::OnDBServer, Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Each batch in a dump can grow to at most this size.)");

  prgOptions
      ->addOption(
          "--dump.max-batch-size",
          "Maximum batch size value (in bytes) that can be used in a dump.",
          new UInt64Parameter(&_options.batchSizeUpperBound, 1,
                              /*minimum*/ _options.batchSizeLowerBound),
          makeFlags(Flags::Uncommon, Flags::DefaultNoComponents,
                    Flags::OnDBServer, Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Each batch in a dump can grow to at most this size.)");

  prgOptions
      ->addOption(
          "--dump.max-parallelism",
          "Maximum parallelism that can be used in a dump.",
          new UInt64Parameter(&_options.parallelismUpperBound, 1,
                              /*minimum*/ _options.parallelismLowerBound),
          makeFlags(Flags::Uncommon, Flags::DefaultNoComponents,
                    Flags::OnDBServer, Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(R"(Each dump action on a server can use at most
this many parallel threads. Note that end users can still start multiple
dump actions that run in parallel.)");
}

void DumpLimitsOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions>& /*prgOptions*/) {
  if (_options.batchSizeLowerBound > _options.batchSizeUpperBound) {
    LOG_TOPIC("79c1b", FATAL, arangodb::Logger::CONFIG)
        << "invalid value for --dump.max-batch-size. Please use a value "
        << "of at least " << _options.batchSizeLowerBound;
    FATAL_ERROR_EXIT();
  }

  if (_options.parallelismLowerBound > _options.parallelismUpperBound) {
    LOG_TOPIC("f433c", FATAL, arangodb::Logger::CONFIG)
        << "invalid value for --dump.max-parallelism. Please use a value "
        << "of at least " << _options.parallelismLowerBound;
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb

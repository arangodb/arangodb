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

#include "TemporaryStorageOptionsProvider.h"

#include <filesystem>

#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void TemporaryStorageOptionsProvider::declareOptions(
    std::shared_ptr<options::ProgramOptions>& prgOptions) {
  prgOptions
      ->addOption(
          "--temp.intermediate-results-path",
          "The path for storing ephemeral, intermediate results on disk "
          "(empty = not used).",
          new StringParameter(&_options.basePath),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000)
      .setLongDescription(R"(Queries can store intermediate and final results
temporarily on disk if a specified threshold is exceeded, to decrease the memory
usage. Specify a path to a directory for the temporary data to activate the
spillover feature. The directory must not be located underneath the instance's
database directory.

The threshold value to start spilling data onto disk is either a number of rows
produced by a query or an amount of memory used in bytes, which you can set as
query options (`spillOverThresholdNumRows` and `spillOverThresholdMemoryUsage`).

**Note**: This feature is experimental and is turned off by default.
Also, the query results are still built up entirely in memory on Coordinators
and single servers for non-streaming queries. To avoid the buildup of the entire
query result in RAM, use a streaming query.)");

  prgOptions
      ->addOption("--temp.intermediate-results-capacity",
                  "The maximum capacity (in bytes) to use for ephemeral, "
                  "intermediate results on disk (0 = unlimited).",
                  new UInt64Parameter(&_options.maxDiskCapacity),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);

  prgOptions
      ->addOption(
          "--temp.intermediate-results-spillover-threshold-num-rows",
          "The number of result rows after which a spillover from RAM to disk "
          "happens for intermediate results (threshold per query executor).",
          new SizeTParameter(&_options.spillOverThresholdNumRows),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);

  prgOptions
      ->addOption(
          "--temp.intermediate-results-spillover-threshold-memory-usage",
          "The memory usage threshold (in bytes) after which a spillover from "
          "RAM to disk happens for intermediate results "
          "(threshold per query executor).",
          new SizeTParameter(&_options.spillOverThresholdMemoryUsage),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);

#ifdef USE_ENTERPRISE
  prgOptions
      ->addOption("--temp.intermediate-results-encryption",
                  "Encrypt ephemeral, intermediate results on disk.",
                  new BooleanParameter(&_options.useEncryption),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Enterprise,
                      arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);

  prgOptions
      ->addOption(
          "--temp.intermediate-results-encryption-hardware-acceleration",
          "Use Intel intrinsics-based encryption, requiring a CPU with "
          "the AES-NI instruction set. "
          "If turned off, then OpenSSL is used, which may use "
          "hardware-accelerated encryption, too.",
          new BooleanParameter(&_options.allowHWAcceleration),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Enterprise,
              arangodb::options::Flags::Experimental))
      .setIntroducedIn(31000);
#endif
}

void TemporaryStorageOptionsProvider::validateOptions(
    std::shared_ptr<options::ProgramOptions>& /*opts*/) {
  if (_options.basePath.empty()) {
    return;
  }
  // replace $PID with current process id
  _options.basePath = basics::StringUtils::replace(
      _options.basePath, "$PID", std::to_string(Thread::currentProcessId()));
  // Note: path normalization + "basePath must not be inside --database.directory"
  // live in TemporaryStorageFeature::prepare(), since DatabasePathFeature is
  // only added to the server after all providers have run.
}

}  // namespace arangodb

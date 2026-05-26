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

#include "LogBufferOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void LogBufferOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, LogBufferFeatureOptions& opts) {
  options
      ->addOption("--log.in-memory",
                  "Use an in-memory log appender which can be queried via the "
                  "API and web interface.",
                  new BooleanParameter(&opts.useInMemoryAppender),
                  makeDefaultFlags(Flags::Uncommon))
      .setIntroducedIn(30800)
      .setLongDescription(R"(You can use this option to toggle storing log
messages in memory, from which they can be consumed via the `/_admin/log`
HTTP API and via the web interface.

By default, this option is turned on, so log messages are consumable via the API
and web interface. Turning this option off disables that functionality, saves a
bit of memory for the in-memory log buffers, and prevents potential log
information leakage via these means.)");

  std::unordered_set<std::string> const logLevels = {
      "fatal", "error", "err", "warning", "warn", "info", "debug", "trace"};
  options
      ->addOption(
          "--log.in-memory-level",
          "Use an in-memory log appender only for this log level and higher.",
          new DiscreteValuesParameter<StringParameter>(
              &opts.minInMemoryLogLevel, logLevels),
          makeDefaultFlags(Flags::Uncommon))
      .setLongDescription(R"(You can use this option to control which log
messages are preserved in memory (in case `--log.in-memory` is enabled).

The default value is `info`, meaning all log messages of types `info`,
`warning`, `error`, and `fatal` are stored in-memory by an instance. By setting
this option to `warning`, only `warning`, `error` and `fatal` log messages are
preserved in memory, and by setting the option to `error`, only `error` and
`fatal` messages are kept.

This option is useful because the number of in-memory log messages is limited
to the latest 2048 messages, and these slots are shared between informational,
warning, and error messages by default.)");
}

}  // namespace arangodb

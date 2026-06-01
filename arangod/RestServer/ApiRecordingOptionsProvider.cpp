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

#include "ApiRecordingOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void ApiRecordingOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> opts, ApiRecordingFeatureOptions& options) {
  opts->addOption(
      "--server.api-call-recording",
      "Whether to record recent API calls for debugging purposes.",
      new BooleanParameter(&options.enabledCalls),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--server.api-recording-memory-limit",
      "Size limit for the list of API call records.",
      new UInt64Parameter(&options.totalMemoryLimitCalls, 1,
                          256 * (std::size_t{1} << 10),  // Min: 256 KiB
                          256 * (std::size_t{1} << 30)   // Max: 256 GiB
                          ),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--server.aql-query-recording",
      "Whether to record recent AQL queries for debugging purposes.",
      new BooleanParameter(&options.enabledQueries),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--server.aql-recording-memory-limit",
      "Size limit for the list of AQL query records.",
      new UInt64Parameter(&options.totalMemoryLimitQueries, 1,
                          256 * (std::size_t{1} << 10),  // Min: 256 KiB
                          256 * (std::size_t{1} << 30)   // Max: 256 GiB
                          ),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
          "--log.recording-api-enabled",
          "Whether the recording API is enabled (true) or not (false), or "
          "only enabled for the superuser (jwt).",
          new StringParameter(&options.apiSwitch))
      .setLongDescription(R"(The `/_admin/server/api-calls` and
`/_admin/server/aql-queries` endpoints provide access to recorded API calls
and AQL queries respectively. They are referred to as the recording API.

Since this data might be sensitive depending on the context of the deployment,
these endpoints need to be properly secured. By default, the recording API is
accessible for admin users (users with administrative access to the `_system`
database). However, you can restrict it further to the superuser or disable it
altogether:

- `true`: The recording API is accessible for admin users.
- `jwt`: The recording API is accessible for the superuser only
  (authentication with JWT superuser token and empty username).
- `false`: The recording API is not accessible at all.

Whether API calls and AQL queries are recorded is independent of this option.
It is controlled by the `--server.api-call-recording` and
`--server.aql-query-recording` startup options.)");
}

void ApiRecordingOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> opts, ApiRecordingFeatureOptions& options) {
  if (options.apiSwitch == "true" || options.apiSwitch == "on" ||
      options.apiSwitch == "On") {
    options.apiEnabled = true;
    options.apiSwitch = "true";
  } else if (options.apiSwitch == "jwt" || options.apiSwitch == "JWT") {
    options.apiEnabled = true;
    options.apiSwitch = "jwt";
  } else {
    options.apiEnabled = false;
    options.apiSwitch = "false";
  }
}

}  // namespace arangodb

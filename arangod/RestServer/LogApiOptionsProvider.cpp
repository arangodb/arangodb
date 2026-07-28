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

#include "LogApiOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

void LogApiOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> prgOpts, LogApiOptions& apiOpts) {
  prgOpts
      ->addOption("--log.api-enabled",
                  "Whether the log API is enabled (true) or not (false), or "
                  "only enabled for the superuser (jwt).",
                  new options::StringParameter(&apiOpts.apiSwitch))
      .setLongDescription(R"(Credentials are not written to log files.
        Nevertheless, some logged data might be sensitive depending on the context of
        the deployment. For example, if request logging is switched on, user requests
        and corresponding data might end up in log files. Therefore, a certain care
        with log files is recommended.
        
        Since the database server offers an API to control logging and query logging
        data, this API has to be secured properly. By default, the API is accessible
        for admin users (administrative access to the `_system` database).
        However, you can restrict it further to the superuser or disable it altogether:
        
         - `true`: The `/_admin/log` API is accessible for admin users.
         - `jwt`: The `/_admin/log` API is accessible for the superuser only
           (authentication with JWT superuser token and empty username).
         - `false`: The `/_admin/log` API is not accessible at all.)");
}

void LogApiOptionsProvider::validateOptionsImpl(
    std::shared_ptr<options::ProgramOptions> /*prgOpts*/,
    LogApiOptions& apiOpts) {
  if (apiOpts.apiSwitch == "true" || apiOpts.apiSwitch == "on" ||
      apiOpts.apiSwitch == "On") {
    apiOpts.apiEnabled = true;
    apiOpts.apiSwitch = "true";
  } else if (apiOpts.apiSwitch == "jwt" || apiOpts.apiSwitch == "JWT") {
    apiOpts.apiEnabled = true;
    apiOpts.apiSwitch = "jwt";
  } else {
    apiOpts.apiEnabled = false;
    apiOpts.apiSwitch = "false";
  }
}

}  // namespace arangodb
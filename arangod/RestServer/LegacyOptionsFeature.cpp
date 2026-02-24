////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/LegacyOptionsFeature.h"

#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb;
using namespace arangodb::options;

LegacyOptionsFeature::LegacyOptionsFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature{server, *this} {
  setOptional(false);
}

void LegacyOptionsFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "JavaScript engine");
  options->addSection("foxx", "Foxx services");

  // V8DealerFeature options
  options->addObsoleteOption("--javascript.gc-frequency",
                             "JavaScript GC frequency (each x-th request)",
                             true);
  options->addObsoleteOption("--javascript.gc-interval",
                             "JavaScript GC interval (each x seconds)", true);
  options->addObsoleteOption("--javascript.app-path",
                             "directory for Foxx applications", true);
  options->addObsoleteOption("--javascript.startup-directory",
                             "path to the directory containing JavaScript "
                             "startup scripts",
                             true);
  options->addObsoleteOption("--javascript.module-directory",
                             "additional paths containing JavaScript modules",
                             true);
  options->addObsoleteOption("--javascript.copy-installation",
                             "copy contents of 'javascript.startup-directory'",
                             false);
  options->addObsoleteOption("--javascript.v8-contexts",
                             "maximum number of V8 contexts", true);
  options->addObsoleteOption("--javascript.v8-contexts-minimum",
                             "minimum number of V8 contexts to keep", true);
  options->addObsoleteOption("--javascript.v8-contexts-max-invocations",
                             "maximum number of invocations for each V8 "
                             "context before it is disposed",
                             true);
  options->addObsoleteOption("--javascript.v8-contexts-max-age",
                             "maximum lifetime for each V8 context (in "
                             "seconds) before it is disposed",
                             true);
  options->addObsoleteOption("--javascript.allow-admin-execute",
                             "for testing purposes allow /_admin/execute",
                             false);
  options->addObsoleteOption("--javascript.transactions",
                             "enable JavaScript transactions", false);
  options->addObsoleteOption("--javascript.user-defined-functions",
                             "enable JavaScript user-defined functions", false);
  options->addObsoleteOption("--javascript.tasks", "enable JavaScript tasks",
                             false);
  options->addObsoleteOption("--javascript.enabled",
                             "enable the V8 JavaScript engine", false);

  // V8SecurityFeature options
  options->addObsoleteOption("--javascript.allow-port-testing",
                             "allow testing of ports from within JavaScript "
                             "actions",
                             false);
  options->addObsoleteOption("--javascript.allow-external-process-control",
                             "allow controlling external processes from within "
                             "JavaScript actions",
                             false);
  options->addObsoleteOption("--javascript.harden", "harden JavaScript engine",
                             false);
  options->addObsoleteOption("--javascript.startup-options-allowlist",
                             "startup options whose names match this regular "
                             "expression are allowed",
                             true);
  options->addObsoleteOption("--javascript.startup-options-denylist",
                             "startup options whose names match this regular "
                             "expression are denied",
                             true);
  options->addObsoleteOption("--javascript.environment-variables-allowlist",
                             "environment variables that are accessible from "
                             "within JavaScript",
                             true);
  options->addObsoleteOption("--javascript.environment-variables-denylist",
                             "environment variables that are not accessible "
                             "from within JavaScript",
                             true);
  options->addObsoleteOption("--javascript.endpoints-allowlist",
                             "endpoints that can be connected to from within "
                             "JavaScript",
                             true);
  options->addObsoleteOption("--javascript.endpoints-denylist",
                             "endpoints that cannot be connected to from "
                             "within JavaScript",
                             true);
  options->addObsoleteOption("--javascript.files-allowlist",
                             "filesystem paths accessible from within "
                             "JavaScript",
                             true);

  // V8PlatformFeature options
  options->addObsoleteOption("--javascript.v8-max-heap",
                             "The maximal heap size (in MiB).", true);
  options->addObsoleteOption("--javascript.v8-options",
                             "Options to pass to V8.", true);

  // FoxxFeature options
  options->addObsoleteOption("--foxx.queues", "enable Foxx queues", false);
  options->addObsoleteOption("--foxx.queues-poll-interval",
                             "poll interval for Foxx queue manager (in "
                             "seconds)",
                             true);
  options->addObsoleteOption("--foxx.force-update-on-startup",
                             "ensure all Foxx services are up-to-date at "
                             "start",
                             false);
  options->addObsoleteOption("--foxx.enable", "enable Foxx", false);

  // ServerSecurityFeature Foxx options
  options->addObsoleteOption(
      "--foxx.api", "Whether to enable the Foxx management REST APIs.", false);
  options->addObsoleteOption("--foxx.store",
                             "Whether to enable the Foxx store in the web "
                             "interface.",
                             false);
  options->addObsoleteOption("--foxx.allow-install-from-remote",
                             "Allow installing Foxx apps from remote URLs "
                             "other than GitHub.",
                             false);

  // ScriptFeature options
  options->addObsoleteOption("--javascript.script-parameter",
                             "Script parameter.", true);

  // ServerFeature options
  options->addObsoleteOption("--console",
                             "Start the server with a JavaScript emergency "
                             "console.",
                             false);
  options->addObsoleteOption("--javascript.script", "Run the script and exit.",
                             true);

  // FrontendFeature options
  options->addObsoleteOption("--web-interface.version-check",
                             "Alert the user if new versions are available.",
                             false);
}

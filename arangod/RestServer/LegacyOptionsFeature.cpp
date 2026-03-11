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
  // V8DealerFeature options
  options->addObsoleteOption("--javascript.gc-frequency",
                             "Time-based garbage collection frequency for "
                             "JavaScript objects (each x seconds).",
                             true);
  options->addObsoleteOption(
      "--javascript.gc-interval",
      "Request-based garbage collection interval for JavaScript objects "
      "(each x requests).",
      true);
  options->addObsoleteOption("--javascript.app-path",
                             "The directory for Foxx applications.", true);
  options->addObsoleteOption(
      "--javascript.startup-directory",
      "A path to the directory containing the JavaScript startup scripts.",
      true);
  options->addObsoleteOption("--javascript.module-directory",
                             "Additional paths containing JavaScript modules.",
                             true);
  options->addObsoleteOption(
      "--javascript.copy-installation",
      "Copy the contents of `javascript.startup-directory` on first start.",
      false);
  options->addObsoleteOption(
      "--javascript.v8-contexts",
      "The maximum number of V8 contexts that are created for "
      "executing JavaScript actions.",
      true);
  options->addObsoleteOption(
      "--javascript.v8-contexts-minimum",
      "The minimum number of V8 contexts to keep available for "
      "executing JavaScript actions.",
      true);
  options->addObsoleteOption(
      "--javascript.v8-contexts-max-invocations",
      "The maximum number of invocations for each V8 context before it is "
      "disposed (0 = unlimited).",
      true);
  options->addObsoleteOption(
      "--javascript.v8-contexts-max-age",
      "The maximum age for each V8 context (in seconds) before it "
      "is disposed.",
      true);
  options->addObsoleteOption(
      "--javascript.allow-admin-execute",
      "For testing purposes, allow `/_admin/execute`. Never enable "
      "this option in production!",
      false);
  options->addObsoleteOption("--javascript.transactions",
                             "Enable JavaScript transactions.", false);
  options->addObsoleteOption(
      "--javascript.user-defined-functions",
      "Enable JavaScript user-defined functions (UDFs) in AQL queries.", false);
  options->addObsoleteOption("--javascript.tasks", "Enable JavaScript tasks.",
                             false);
  options->addObsoleteOption("--javascript.enabled",
                             "Enable the V8 JavaScript engine.", false);

  // V8SecurityFeature options
  options->addObsoleteOption(
      "--javascript.allow-port-testing",
      "Allow the testing of ports from within JavaScript actions.", false);
  options->addObsoleteOption(
      "--javascript.allow-external-process-control",
      "Allow the execution and control of external processes from "
      "within JavaScript actions.",
      false);
  options->addObsoleteOption(
      "--javascript.harden",
      "Disable access to JavaScript functions in the internal "
      "module: getPid() and logLevel().",
      false);
  options->addObsoleteOption(
      "--javascript.startup-options-allowlist",
      "Startup options whose names match this regular "
      "expression are allowed and exposed to JavaScript.",
      true);
  options->addObsoleteOption(
      "--javascript.startup-options-denylist",
      "Startup options whose names match this regular "
      "expression are not exposed (if not in the allowlist) to "
      "JavaScript actions.",
      true);
  options->addObsoleteOption(
      "--javascript.environment-variables-allowlist",
      "Environment variables that are accessible in JavaScript.", true);
  options->addObsoleteOption("--javascript.environment-variables-denylist",
                             "Environment variables that are inaccessible in "
                             "JavaScript (if not in the allowlist).",
                             true);
  options->addObsoleteOption(
      "--javascript.endpoints-allowlist",
      "Endpoints that can be connected to via the "
      "`@arangodb/request` module in JavaScript actions.",
      true);
  options->addObsoleteOption("--javascript.endpoints-denylist",
                             "Endpoints that cannot be connected to via the "
                             "`@arangodb/request` module in JavaScript actions "
                             "(if not in the allowlist).",
                             true);
  options->addObsoleteOption("--javascript.files-allowlist",
                             "Filesystem paths that are accessible from within "
                             "JavaScript actions.",
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
  options->addObsoleteOption(
      "--foxx.api", "whether to enable the Foxx management REST APIs", false);
  options->addObsoleteOption(
      "--foxx.store", "whether to enable the Foxx store in the web interface",
      false);
  options->addObsoleteOption(
      "--foxx.allow-install-from-remote",
      "allow installing Foxx apps from remote URLs other than GitHub", true);

  // ActionFeature options
  options->addObsoleteOption("--server.allow-use-database",
                             "Allow to change the database in REST actions.",
                             false);

  // AuthenticationFeature options
  options->addObsoleteOption(
      "--server.authentication-system-only",
      "Use HTTP authentication only for requests to /_api and /_admin "
      "endpoints.",
      false);
  options->addOldOption("server.authenticate-system-only",
                        "server.authentication-system-only");

  // StatisticsFeature options
  options->addObsoleteOption("--server.statistics-history",
                             "Whether to store statistics in the database.",
                             false);

  // ScriptFeature options
  options->addObsoleteOption("--javascript.script-parameter",
                             "Script parameter.", true);

  options->addObsoleteOption("--javascript.script", "Run the script and exit.",
                             true);

  // FrontendFeature options
  options->addObsoleteOption("--web-interface.version-check",
                             "Alert the user if new versions are available.",
                             false);
}

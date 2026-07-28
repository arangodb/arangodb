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

#include "V8SecurityOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void V8SecurityOptionsProvider::declareOptions(
    std::shared_ptr<options::ProgramOptions> options,
    V8SecurityFeatureOptions& opts) {
  options->addSection("javascript", "JavaScript engine and execution");
  options->addOption("--javascript.allow-port-testing",
                     "Allow the testing of ports from within JavaScript.",
                     new BooleanParameter(&opts.allowPortTesting),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle,
                         arangodb::options::Flags::Uncommon));

  options->addOption(
      "--javascript.allow-external-process-control",
      "Allow the execution and control of external processes from "
      "within JavaScript.",
      new BooleanParameter(&opts.allowProcessControl),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle,
          arangodb::options::Flags::Uncommon));

  options->addOption("--javascript.harden",
                     "Disable access to JavaScript functions in the internal "
                     "module: getPid() and logLevel().",
                     new BooleanParameter(&opts.hardenInternalModule),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.startup-options-allowlist",
      "Startup options whose names match this regular expression are allowed "
      "and exposed to JavaScript contexts.",
      new VectorParameter<StringParameter>(&opts.startupOptionsAllowList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.startup-options-denylist",
      "Startup options whose names match this regular expression are not "
      "exposed to JavaScript contexts (overriding the allowlist).",
      new VectorParameter<StringParameter>(&opts.startupOptionsDenyList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.environment-variables-allowlist",
      "Environment variables whose name match this regular expression are "
      "accessible in JavaScript contexts.",
      new VectorParameter<StringParameter>(&opts.environmentVariablesAllowList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.environment-variables-denylist",
      "Environment variables whose name match this regular expression are "
      "inaccessible in JavaScript (overriding the allowlist).",
      new VectorParameter<StringParameter>(&opts.environmentVariablesDenyList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.endpoints-allowlist",
      "URLs that match this regular expression can be connected to via the "
      "`@arangodb/request` module in JavaScript contexts.",
      new VectorParameter<StringParameter>(&opts.endpointsAllowList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.endpoints-denylist",
      "URLs that match this regular expression cannot be connected to via the "
      "`@arangodb/request` module in JavaScript contexts (overriding the "
      "allowlist).",
      new VectorParameter<StringParameter>(&opts.endpointsDenyList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption("--javascript.files-allowlist",
                     "Filesystem paths that match this regular expression are "
                     "accessible in JavaScript contexts.",
                     new VectorParameter<StringParameter>(&opts.filesAllowList),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.files-denylist",
      "Filesystem paths that match this regular expression are inaccessible in "
      "JavaScript contexts (overriding the allowlist).",
      new VectorParameter<StringParameter>(&opts.filesDenyList),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOldOption("--javascript.startup-options-whitelist",
                        "--javascript.startup-options-allowlist");
  options->addOldOption("--javascript.startup-options-blacklist",
                        "--javascript.startup-options-denylist");
  options->addOldOption("--javascript.environment-variables-whitelist",
                        "--javascript.environment-variables-allowlist");
  options->addOldOption("--javascript.environment-variables-blacklist",
                        "--javascript.environment-variables-denylist");
  options->addOldOption("--javascript.endpoints-whitelist",
                        "--javascript.endpoints-allowlist");
  options->addOldOption("--javascript.endpoints-blacklist",
                        "--javascript.endpoints-denylist");
  options->addOldOption("--javascript.files-whitelist",
                        "--javascript.files-allowlist");
}

}  // namespace arangodb

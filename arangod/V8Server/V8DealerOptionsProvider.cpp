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

#include "V8DealerOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void V8DealerOptionsProvider::declareOptions(
    std::shared_ptr<options::ProgramOptions> options,
    V8DealerFeatureOptions& opts) {
  options->addSection("javascript", "JavaScript engine and execution");

  options
      ->addOption(
          "--javascript.gc-frequency",
          "Time-based garbage collection frequency for JavaScript objects "
          "(each x seconds).",
          new DoubleParameter(&opts.gcFrequency),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(This option is useful to have the garbage
collection still work in periods with no or little numbers of requests.)");

  options->addOption(
      "--javascript.gc-interval",
      "Request-based garbage collection interval for JavaScript objects "
      "(each x requests).",
      new UInt64Parameter(&opts.gcInterval),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle,
          arangodb::options::Flags::Uncommon));

  options->addOption("--javascript.app-path",
                     "The directory for Foxx applications.",
                     new StringParameter(&opts.appPath),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.startup-directory",
      "A path to the directory containing the JavaScript startup scripts.",
      new StringParameter(&opts.startupDirectory),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--javascript.module-directory",
      "Additional paths containing JavaScript modules.",
      new VectorParameter<StringParameter>(&opts.moduleDirectories),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle,
          arangodb::options::Flags::Uncommon));

  options
      ->addOption(
          "--javascript.copy-installation",
          "Copy the contents of `javascript.startup-directory` on first start.",
          new BooleanParameter(&opts.copyInstallation),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(This option is intended to be useful for rolling
upgrades. If you set it to `true`, you can upgrade the underlying ArangoDB
packages without influencing the running _arangod_ instance.

Setting this value does only make sense if you use ArangoDB outside of a
container solution, like Docker or Kubernetes.)");

  options
      ->addOption("--javascript.v8-contexts",
                  "The maximum number of V8 contexts that are created for "
                  "executing JavaScript actions.",
                  new UInt64Parameter(&opts.nrMaxExecutors),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Dynamic,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(More contexts allow executing more JavaScript
actions in parallel, provided that there are also enough threads available.
Note that each V8 context uses a substantial amount of memory and requires
periodic CPU processing time for garbage collection.

This option configures the maximum number of V8 contexts that can be used in
parallel. On server start, only as many V8 contexts are created as are
configured by the `--javascript.v8-contexts-minimum` option. The actual number
of available V8 contexts may vary between `--javascript.v8-contexts-minimum`
and `--javascript.v8-contexts` at runtime. When there are unused V8 contexts
that linger around, the server's garbage collector thread automatically deletes
them.)");

  options
      ->addOption("--javascript.v8-contexts-minimum",
                  "The minimum number of V8 contexts to keep available for "
                  "executing JavaScript actions.",
                  new UInt64Parameter(&opts.nrMinExecutors),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(The actual number of V8 contexts never drops below
this value, but it may go up as high as specified by the
`--javascript.v8-contexts` option.

When there are unused V8 contexts that linger around and the number of V8
contexts is greater than `--javascript.v8-contexts-minimum`, the server's
garbage collector thread automatically deletes them.)");

  options->addOption(
      "--javascript.v8-contexts-max-invocations",
      "The maximum number of invocations for each V8 context before it is "
      "disposed (0 = unlimited).",
      new UInt64Parameter(&opts.maxExecutorInvocations),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle,
          arangodb::options::Flags::Uncommon));

  options
      ->addOption("--javascript.v8-contexts-max-age",
                  "The maximum age for each V8 context (in seconds) before it "
                  "is disposed.",
                  new DoubleParameter(&opts.maxExecutorAge),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(If both `--javascript.v8-contexts-max-invocations`
and `--javascript.v8-contexts-max-age` are set, then the context is destroyed
when either of the specified threshold values is reached.)");

  options
      ->addOption("--javascript.allow-admin-execute",
                  "For testing purposes, allow `/_admin/execute`. Never enable "
                  "this option "
                  "in production!",
                  new BooleanParameter(&opts.allowAdminExecute),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(You can use this option to control whether
user-defined JavaScript code is allowed to be executed on the server by sending
HTTP requests to the `/_admin/execute` API endpoint with an authenticated user
account.

The default value is `false`, which disables the execution of user-defined
code. This is also the recommended setting for production. In test environments,
it may be convenient to turn the option on in order to send arbitrary setup
or teardown commands for execution on the server.)");

  options
      ->addOption("--javascript.transactions",
                  "Enable JavaScript transactions.",
                  new BooleanParameter(&opts.allowJavaScriptTransactions),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30800);

  options
      ->addOption(
          "--javascript.user-defined-functions",
          "Enable JavaScript user-defined functions (UDFs) in AQL queries.",
          new BooleanParameter(&opts.allowJavaScriptUdfs),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31004);

  options
      ->addOption("--javascript.tasks", "Enable JavaScript tasks.",
                  new BooleanParameter(&opts.allowJavaScriptTasks),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30800);

  options
      ->addOption("--javascript.enabled", "Enable the V8 JavaScript engine.",
                  new BooleanParameter(&opts.enableJS),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(By default, the V8 engine is enabled on single
servers and Coordinators. It is disabled by default on Agents and DB-Servers.

It is possible to turn the V8 engine off also on the latter instance types to
reduce the footprint of ArangoDB. Turning the V8 engine off on single servers or
Coordinators will automatically render certain functionality unavailable or
dysfunctional. The affected functionality includes JavaScript transactions, Foxx,
AQL user-defined functions, the built-in web interface and some server APIs.)");
}

}  // namespace arangodb

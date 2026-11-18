/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// //////////////////////////////////////////////////////////////////////////////

// What any test suite needs in order to run against RBAC.
//
// `--rbac` is a *global* option: instance-manager.js declares it and
// instance.js points every arangod it starts at the service. But pointing
// arangod at a service is not enough to make authorization happen, and the
// three things that are needed are the same for every suite, which is why they
// live here rather than in one suite's file.

const fs = require('fs');
const tu = require('@arangodb/testutils/test-utils');
const executeExternalAndWait = require('internal').executeExternalAndWait;

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;

// `--rbac true` launches the built-in dummy (utils/rbac_dummy.py), which allows
// everything and serves no management API. Only a URL means a real sidecar, and
// only then is there anything to bootstrap or seed through.
function usesRealSidecar (options) {
  return typeof options.rbac === 'string';
}

function rbacDir (options) {
  if (!options.rtaRbacDir) {
    options.rtaRbacDir = fs.join(fs.makeAbsolute('.'), 'tests', 'api', 'rbac', 'rta');
  }
  return options.rtaRbacDir;
}

// //////////////////////////////////////////////////////////////////////////////
// / Server options every suite needs when running under --rbac.
// /
// / Must be applied to `serverOptions` *before* the instanceManager is
// / constructed: its constructor runs handleJWT(), which reads
// / addArgs['server.jwt-secret'] to set `jwt_secret` - and bootstrapUser()
// / below needs that secret to sign its tokens.
// //////////////////////////////////////////////////////////////////////////////
function applyServerOptions (options, serverOptions) {
  if (!options.rbac) {
    return;
  }
  // RBAC is only consulted for authenticated requests: ExecContext::create
  // returns AuthMode::Disabled when the authentication feature is off, and then
  // no policy is ever evaluated whatever --server.external-rbac-service says.
  // Merging testServerAuthInfo into `options` alone - which is what the client
  // side does - never reaches arangod's own arguments.
  Object.assign(serverOptions, tu.testServerAuthInfo);

  // arangod reaches the RBAC service through network::sendRequest, so it
  // applies its *internal* request compression to a third-party endpoint that
  // never negotiated it. etc/testing/arangod-common.conf sets
  // `network.compression-method = auto`, and a well-compressing authorization
  // request then goes out as `content-encoding: x-arango-lz4`, which the
  // sidecar cannot decode; arangod reports the resulting 400 as
  // `bad parameter`. arangod's own default is `none`.
  serverOptions['network.compression-method'] = 'none';
}

// //////////////////////////////////////////////////////////////////////////////
// / Run the python scenario driver with the sanitizers set up around it.
// / The equivalent of test-helper.js' executeExternalAndWaitWithSanitizer,
// / but it cannot be used here because it collects by PID
// //////////////////////////////////////////////////////////////////////////////
function runScenarioDriver (options, instanceManager, argv) {
  const sanHandler = require('@arangodb/testutils/san-file-handler').sanHandler;
  const enabled = options.isSan || options.isCov;

  let sh = null;
  if (enabled) {
    sh = new sanHandler('arangosh', options);
    sh.detectLogfiles(instanceManager.rootDir, instanceManager.rootDir);
  }
  const rc = executeExternalAndWait('python3', argv, false, 0,
                                    sh ? sh.getSanOptions() : []);
  if (sh) {
    collectReports(options, sh, instanceManager.rootDir);
  }
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / Harvest the sanitizer reports the arangosh grandchildren left behind.
// //////////////////////////////////////////////////////////////////////////////
function collectReports (options, sh, rootDir) {
  const sanHandler = require('@arangodb/testutils/san-file-handler').sanHandler;
  const handlers = { 'arangosh': sh };
  let found = false;
  fs.list(rootDir).forEach(name => {
    const m = name.match(/^(?:tsan|alubsan)\.log\.(.+)\.(\d+)$/);
    if (m === null) {
      return;
    }
    const exe = m[1];
    if (!handlers.hasOwnProperty(exe)) {
      handlers[exe] = new sanHandler(exe, options);
      handlers[exe].detectLogfiles(rootDir, rootDir);
    }
    found = handlers[exe].fetchSanFileAfterExit(parseInt(m[2], 10)) || found;
  });
  return found;
}

// Flags common to every run_scenarios.py invocation.
function runnerArgs (options, instanceManager) {
  const secretFile = fs.join(instanceManager.rootDir, 'rta_rbac_jwt_secret');
  // mkjwt.py needs the raw HMAC signing secret. `instanceManager.JWT` is an
  // already-encoded superuser token, and signing with it mints tokens arangod
  // rejects with errorNum 11 - which the scenario runner then reads as "not an
  // RBAC deployment". Assert rather than fall back: a silent fallback is what
  // let this break unnoticed when the field was renamed.
  const secret = instanceManager.jwt_secret;
  if (typeof secret !== 'string' || secret === '') {
    throw new Error(
      'instanceManager.jwt_secret is empty - cannot sign RBAC tokens. ' +
      '--rbac requires authentication to be enabled on the server.');
  }
  fs.makeDirectoryRecursive(instanceManager.rootDir);
  fs.write(secretFile, secret);
  return [
    fs.join(rbacDir(options), 'run_scenarios.py'),
    '--management', options.rbac,
    '--integration', options.rbac,
    '--jwt-secret-file', secretFile,
    // A URL means someone else runs the sidecar; the runner must not restart it
    // into a different authorization mode.
    '--external-sidecar',
  ];
}

// //////////////////////////////////////////////////////////////////////////////
// / Give the account the suite runs as an allow-all binding, and take it away
// / again afterwards.
// /
// / A real sidecar denies by default, so that account cannot complete even
// / arangosh's connect handshake until it is bound. Production does the same
// / thing: the operator binds `root` to managed:predefined:super-admin on
// / bootstrap. The built-in dummy allows everything, so this is a no-op there.
// //////////////////////////////////////////////////////////////////////////////
function bootstrapUser (options, instanceManager, remove) {
  if (!usesRealSidecar(options)) {
    return true;
  }
  const flag = remove ? '--remove-bootstrap-user' : '--bootstrap-user';
  const argv = runnerArgs(options, instanceManager).concat([flag, options.username]);
  const rc = runScenarioDriver(options, instanceManager, argv);
  if (rc.exit !== 0 && !remove) {
    print(`${RED}${(new Date()).toISOString()} could not give ` +
          `'${options.username}' an RBAC binding; the workload cannot run ` +
          `against a deny-by-default sidecar without one${RESET}`);
    return false;
  }
  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / Refuse to run a suite against a real sidecar unless it has been verified
// / against one.
// /
// / Because --rbac is global, any suite accepts it - and would then run against
// / a deny-by-default authorization service it has never been exercised
// / against. The failure mode is a confusing pile of denials, or worse, a green
// / run that proves nothing. Better to say so.
// //////////////////////////////////////////////////////////////////////////////
function checkSuiteSupported (options, suiteName, verified) {
  if (!usesRealSidecar(options) || verified || options.rbacUnverified) {
    return null;
  }
  return `'${suiteName}' has not been verified against a real RBAC sidecar, ` +
    `and --rbac ${options.rbac} would run it against one that denies by ` +
    `default. Set rbacVerified on its runner once it has been checked, or ` +
    `pass --rbacUnverified true to try it anyway.`;
}

exports.runScenarioDriver = runScenarioDriver;
exports.usesRealSidecar = usesRealSidecar;
exports.applyServerOptions = applyServerOptions;
exports.runnerArgs = runnerArgs;
exports.bootstrapUser = bootstrapUser;
exports.checkSuiteSupported = checkSuiteSupported;
exports.registerOptions = function (optionsDefaults, optionsDocumentation) {
  tu.CopyIntoObject(optionsDefaults, {
    'rbacUnverified': false,
  });
  tu.CopyIntoList(optionsDocumentation, [
    ' RBAC:',
    '   - `rbacUnverified`: run a suite against a real RBAC sidecar even though',
    '     it has not been verified against one',
    ''
  ]);
};

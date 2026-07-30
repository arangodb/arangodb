/* jshint strict: false, sub: true */
/* global arango */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief observe which authorization questions a RestHandler asks
// /
// / `ExecContext::can()` traces every authorization question on the
// / `authorization` log topic as
// /
// /   AUTHZ-CHECK UseCollection db=_system name=foo level=read
// /
// / This module raises that topic to TRACE and collects everything that was
// / logged in between:
// /
// /   const {
// /     beginObserve, endObserve, assertPermissions
// /   } = require('@arangodb/testutils/permissions-observer');
// /
// /   beginObserve();
// /   arango.GET_RAW(`/_api/collection/${cn}/count`);
// /   let observed = endObserve();
// /   assertPermissions([`UseCollection db=_system name=${cn} level=read`,
// /                      'UseDatabase name=_system level=read'], observed);
// /
// / Requirements for the test that uses this:
// /
// /  - the server must run with `log.force-direct: 'true'`, so that log
// /    messages are written to the log file synchronously - otherwise the
// /    logging thread may still be holding the interesting lines when
// /    endObserve() reads the file,
// /  - the server must run with `server.authentication: 'true'`; without it
// /    every context is `AuthMode::Disabled` and handlers do not ask at all.
// /    The same holds for an internal (JWT) superuser connection: many
// /    handlers skip their questions for superuser contexts, so reconnect with
// /    basic auth if you want to observe what a regular user triggers,
// /  - questions asked via `ExecContext::canUseHardenedAction()` are only
// /    asked when the server runs with `server.harden: 'true'`,
// /  - only the server the shell is currently talking to is observed;
// /    checks performed on DB servers are not collected.
// //////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const jsunity = require('jsunity');
const { assertEqual } = jsunity.jsUnity.assertions;

// prefix of the log message emitted by ExecContext::can()
const CHECK_MARKER = 'AUTHZ-CHECK ';

// log levels to restore in endObserve(); non-null while observing
let savedLogLevels = null;
// the instance we observe, and how many log lines it had when we started
let observedInstance = null;
let observedFrom = 0;
// whether we already verified that logging is synchronous
let checkedForceDirect = false;

// //////////////////////////////////////////////////////////////////////////////
// / @brief the instance whose log we observe: the one we talk to
// //////////////////////////////////////////////////////////////////////////////

function currentInstance () {
  let im = global.instanceManager;
  if (im === undefined) {
    throw new Error('permissions-observer requires global.instanceManager');
  }
  let endpoint = arango.getEndpoint();
  let arangod = im.arangods.find((arangod) => arangod.endpoint === endpoint);
  if (arangod === undefined) {
    // e.g. after a reconnect via a rewritten endpoint - fall back to the
    // server an arangosh usually talks to
    arangod = im.arangods.find((arangod) => arangod.isFrontend());
  }
  if (arangod === undefined) {
    throw new Error(`no instance found for endpoint ${endpoint}`);
  }
  return arangod;
}

function logLines (arangod) {
  return fs.readFileSync(arangod.logFile, 'ascii').split('\n');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief complain if the server buffers its log messages
// /
// / Without `--log.force-direct` the messages are handed to the logging thread,
// / and there is no way to flush it from the outside - so an observation would
// / silently miss the tail of the log.
// //////////////////////////////////////////////////////////////////////////////

function checkForceDirect () {
  if (checkedForceDirect) {
    return;
  }
  checkedForceDirect = true;
  let options;
  try {
    options = arango.GET('/_admin/options');
  } catch (err) {
    return;  // options API not available here - trust the caller
  }
  if (options['log.force-direct'] === false) {
    throw new Error("the permissions observer needs synchronous logging: add " +
                    "'log.force-direct': 'true' to the test's getOptions()");
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief start observing authorization questions
// //////////////////////////////////////////////////////////////////////////////

function beginObserve () {
  if (savedLogLevels !== null) {
    throw new Error('beginObserve() called twice - endObserve() is missing');
  }
  checkForceDirect();
  observedInstance = currentInstance();
  let levels = arango.GET('/_admin/log/level');
  // the authorization questions of this very request are asked while the topic
  // is still silent, so they are not part of the observation
  arango.PUT('/_admin/log/level', { authorization: 'trace' });
  savedLogLevels = levels;
  observedFrom = logLines(observedInstance).length - 1;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief stop observing, return the questions asked in between
// /
// / e.g. [ 'UseDatabase name=_system level=read',
// /        'UseCollection db=_system name=foo level=read' ]
// //////////////////////////////////////////////////////////////////////////////

function endObserve () {
  if (savedLogLevels === null) {
    throw new Error('endObserve() without a preceding beginObserve()');
  }
  let lines = logLines(observedInstance).slice(observedFrom);

  // restore the log levels only after reading the log, so that the questions
  // of that request are not part of the observation
  disableObserve();

  let permissions = [];
  lines.forEach((line) => {
    let pos = line.indexOf(CHECK_MARKER);
    if (pos !== -1) {
      permissions.push(line.substr(pos + CHECK_MARKER.length).trim());
    }
  });
  return permissions;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief stop observing and restore the log levels, if we are observing
// /
// / Safe to call at any time - call it in tearDown() so that a test failing
// / in the middle of an observation does not leave the log topic at TRACE.
// //////////////////////////////////////////////////////////////////////////////

function disableObserve () {
  if (savedLogLevels === null) {
    return;
  }
  let levels = savedLogLevels;
  savedLogLevels = null;
  arango.PUT('/_admin/log/level', levels);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief deduplicated, sorted permissions, for order-insensitive comparisons
// //////////////////////////////////////////////////////////////////////////////

function permissionSet (permissions) {
  return [...new Set(permissions)].sort();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief assert that exactly the `expected` questions were asked
// //////////////////////////////////////////////////////////////////////////////

function assertPermissions (expected, observed) {
  assertEqual(permissionSet(expected), permissionSet(observed),
              `observed permissions: ${JSON.stringify(observed)}`);
}

exports.beginObserve = beginObserve;
exports.endObserve = endObserve;
exports.disableObserve = disableObserve;
exports.permissionSet = permissionSet;
exports.assertPermissions = assertPermissions;

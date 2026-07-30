/*jshint globalstrict:false, strict:false */
/* global getOptions, arango */

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

// Authorization questions asked by the /_admin/backup endpoint family.
//
// Observation-based counterpart of tests/api/apitests/backup.mjs.
//
// Handler: enterprise/Enterprise/RestHandler/RestHotBackupHandler.cpp
//
// RestHotBackupHandler::verifyPermitted() (called first in execute()) asks, in
// the default mode (--backup.api-enabled=true, which is not "jwt"):
//     canUseAdminAction(AdminBackup)   ->  logs `AdminBackup`
// In jwt mode (--backup.api-enabled=jwt) it instead does an isSuperuserOrDisabled()
// check which asks NOTHING. Our suites run in the default mode, so `AdminBackup`
// is asked. Every request additionally asks `UseDatabase name=_system level=read`
// first in RestHandler::checkUserCanAccess() (the routes have no /_db/ prefix, so
// the database is the connected _system).
//
// AUDIT: enterprise-only endpoints. On a Community Edition build the /_admin/backup
// prefix is not registered, so the requests return 404 (route not found) and only
// the base `UseDatabase name=_system level=read` question fires (if any). The
// AdminBackup question is only asked on an Enterprise build.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false'
  };
}

const jsunity = require('jsunity');
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');

function backupApiAuthzSuite () {
  const useSystem = `UseDatabase name=_system level=read`;
  const adminBackup = `AdminBackup`;

  // remove every hot backup whose id carries one of our test labels
  function cleanupBackups () {
    const res = arango.POST_RAW(`/_db/_system/_admin/backup/list`, {});
    const list =
      (res.parsedBody && res.parsedBody.result && res.parsedBody.result.list)
        ? res.parsedBody.result.list
        : {};
    for (const id of Object.keys(list)) {
      if (id.includes('apitestcreate') || id.includes('apitestdelete')) {
        arango.POST_RAW(`/_db/_system/_admin/backup/delete`, { id });
      }
    }
  }

  // create a backup as root and return its id (undefined on Community build)
  function createBackup (label) {
    const res = arango.POST_RAW(`/_db/_system/_admin/backup/create`, { label });
    return (res.parsedBody && res.parsedBody.result)
      ? res.parsedBody.result.id
      : undefined;
  }

  return {
    tearDown: function () {
      disableObserve();
      cleanupBackups();
    },

    // POST /_admin/backup/list - verifyPermitted() -> canUseAdminAction(AdminBackup)
    testListBackups: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/backup/list`, {});
      assertPermissions([useSystem, adminBackup], endObserve());
    },

    // POST /_admin/backup/create - verifyPermitted() -> canUseAdminAction(AdminBackup)
    testCreateBackup: function () {
      beginObserve();
      const res = arango.POST_RAW(`/_db/_system/_admin/backup/create`,
                                  { label: 'apitestcreate' });
      assertPermissions([useSystem, adminBackup], endObserve());
      // clean up whatever the create produced
      if (res.parsedBody && res.parsedBody.result && res.parsedBody.result.id) {
        arango.POST_RAW(`/_db/_system/_admin/backup/delete`,
                        { id: res.parsedBody.result.id });
      }
    },

    // POST /_admin/backup/delete - verifyPermitted() -> canUseAdminAction(AdminBackup)
    testDeleteBackup: function () {
      const id = createBackup('apitestdelete');
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/backup/delete`, { id: id });
      assertPermissions([useSystem, adminBackup], endObserve());
    },
  };
}

jsunity.run(backupApiAuthzSuite);
return jsunity.done();

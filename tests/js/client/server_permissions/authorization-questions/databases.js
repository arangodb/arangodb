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

// Authorization questions asked by the /_api/database endpoint family.
//
// Observation-based counterpart of tests/api/apitests/databases.mjs.
//
// Handler: arangod/RestHandler/RestDatabaseHandler.cpp
//
// Every request first asks the base `UseDatabase name=<db> level=read` in
// RestHandler::checkUserCanAccess(), where <db> is the database in the request
// path prefix. Beyond that:
//   - GET (list / current / user / shardStatistics) go through
//     methods::Databases::list()/toVelocyPack(), which do NOT call the
//     ExecContext (they read stored auth levels directly), so only the base
//     question is observed.
//   - POST create -> methods::Databases::create() -> canCreateDatabase(name)
//     -> `CreateDatabase name=<name>`.
//   - DELETE -> methods::Databases::drop() -> canDropDatabase(name)
//     -> `DropDatabase name=<name>`.
//
// Create/drop operate on a scratch database 'd2' (recreated inline per test),
// so the fixture database 'd' used by the GET tests is never touched.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false'
  };
}

const jsunity = require('jsunity');
const db = require('@arangodb').db;
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');
const {
  setUpApiTestData,
  tearDownApiTestData,
  DB,
  readUsers,
  singleOnly
} = require('@arangodb/testutils/apitest-fixtures');

function databaseApiAuthzSuite () {
  const useSystem = `UseDatabase name=_system level=read`;
  const useD = `UseDatabase name=${DB} level=read`;

  function dropD2 () {
    arango.DELETE_RAW(`/_db/_system/_api/database/d2`);
  }
  function createD2 () {
    dropD2();
    arango.POST_RAW(`/_db/_system/_api/database`, { name: 'd2' });
  }

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
      dropD2();
    },

    // GET /_db/_system/_api/database - list all databases (_system only);
    // methods::Databases::list() asks nothing, only the base check.
    testListDatabases: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/database`);
      assertPermissions([useSystem], endObserve());
    },

    // GET /_db/d/_api/database/current - _vocbase.toVelocyPack(), no can()
    testCurrentDatabase: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/database/current`);
      assertPermissions([useD], endObserve());
    },

    // GET /_db/d/_api/database/user - Databases::list(user) asks
    // canSeeDatabase() for every database it enumerates
    testUserDatabases: function () {
      const names = db._databases();
      const expected = [useD].concat(
        names.map((n) => `SeeDatabase name=${n}`));
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/database/user`);
      assertPermissions(expected, endObserve());
    },

    // GET /_db/d/_api/database/shardStatistics - on a single server this
    // returns CLUSTER_ONLY_ON_COORDINATOR before any can() is asked.
    // AUDIT: on a cluster coordinator this path reaches ClusterInfo and may
    // ask additional questions.
    testShardStatistics: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/database/shardStatistics`);
      assertPermissions([useD], endObserve());
    },

    // POST /_db/_system/_api/database - create d2 -> canCreateDatabase(d2)
    testCreateDatabase: function () {
      dropD2();
      beginObserve();
      arango.POST_RAW(`/_db/_system/_api/database`, { name: 'd2' });
      assertPermissions([useSystem, `CreateDatabase name=d2`]
                        .concat(readUsers())
                        .concat(['_analyzers', '_appbundles', '_apps',
                                 '_aqlfunctions', '_frontend', '_graphs',
                                 '_jobs', '_queues']
                                .map((n) => `CreateCollection db=d2 name=${n}`))
                        .concat(['_apps', '_jobs'].flatMap((n) => [
                          `UseCollection db=d2 name=${n} level=writemeta`]
                          .concat(singleOnly(
                            [`UseCollection db=d2 name=${n} level=read`])))),
                        endObserve());
      dropD2();
    },

    // DELETE /_db/_system/_api/database/d2 - drop d2 -> canDropDatabase(d2)
    testDropDatabase: function () {
      createD2();
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/database/d2`);
      assertPermissions([useSystem, `DropDatabase name=d2`].concat(readUsers()),
                        endObserve());
    },
  };
}

jsunity.run(databaseApiAuthzSuite);
return jsunity.done();

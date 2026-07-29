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

// Authorization questions asked by the /_api/aqlfunction endpoint family.
//
// Observation-based counterpart of tests/api/apitests/aql-functions.mjs.
//
// Handler: arangod/RestHandler/RestAqlUserFunctionsHandler.cpp (requires V8)
//
// The handler itself asks nothing directly; it delegates to
// VocBase/Methods/AqlUserFunctions.cpp, which stores user-defined AQL functions
// in the system collection `_aqlfunctions`:
//   - GET  (list / list-by-namespace) runs an AQL query
//     `FOR fn IN _aqlfunctions ...` -> READ transaction -> UseCollection(read)
//   - POST (create/replace)  uses a WRITE SingleCollectionTransaction (insert)
//   - DELETE (single name)   uses a WRITE SingleCollectionTransaction (remove)
// A WRITE transaction on a collection asks both the read and the writedata
// question for that collection (cf. documents.js / authorization-questions.js
// testInsertDocument). Every request additionally asks
// `UseDatabase name=<db> level=read` first in RestHandler::checkUserCanAccess().

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true'
  };
}

const jsunity = require('jsunity');
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');
const {
  setUpApiTestData,
  tearDownApiTestData,
  DB
} = require('@arangodb/testutils/apitest-fixtures');

function aqlFunctionApiAuthzSuite () {
  const useD = `UseDatabase name=${DB} level=read`;
  const useSystem = 'UseDatabase name=_system level=read';
  const fnName = 'APITESTNS::APITESTFUNC';
  const fnCode = 'function (a, b) { return a + b; }';
  const fnBody = { name: fnName, code: fnCode, isDeterministic: true };

  // the system collection the functions are stored in
  const aqlFuncD = '_aqlfunctions';
  const readD = `UseCollection db=${DB} name=${aqlFuncD} level=read`;
  const writeD = `UseCollection db=${DB} name=${aqlFuncD} level=writedata`;
  const readSys = `UseCollection db=_system name=${aqlFuncD} level=read`;
  const writeSys = `UseCollection db=_system name=${aqlFuncD} level=writedata`;

  function makeFnD () {
    arango.DELETE_RAW(`/_db/${DB}/_api/aqlfunction/${fnName}`);
    arango.POST_RAW(`/_db/${DB}/_api/aqlfunction`, fnBody);
  }
  function dropFnD () {
    arango.DELETE_RAW(`/_db/${DB}/_api/aqlfunction/${fnName}`);
  }
  function makeFnSys () {
    arango.DELETE_RAW(`/_db/_system/_api/aqlfunction/${fnName}`);
    arango.POST_RAW(`/_db/_system/_api/aqlfunction`, fnBody);
  }
  function dropFnSys () {
    arango.DELETE_RAW(`/_db/_system/_api/aqlfunction/${fnName}`);
  }

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
      dropFnD();
      dropFnSys();
    },

    // GET /_db/d/_api/aqlfunction - AQL query over _aqlfunctions (read trx)
    testListFunctionsD: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/aqlfunction`);
      assertPermissions([useD, readD], endObserve());
    },

    // GET /_db/d/_api/aqlfunction/APITESTNS - same, filtered by namespace
    testListFunctionsByNamespaceD: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/aqlfunction/APITESTNS`);
      assertPermissions([useD, readD], endObserve());
    },

    // POST /_db/d/_api/aqlfunction - WRITE trx (insert) over _aqlfunctions
    // AUDIT: the write goes through a SingleCollectionTransaction opened
    // directly in WRITE mode, so checkCollectionPermission may be invoked only
    // once (writedata) rather than read+writedata as the document handler does
    // (which adds the collection as read and upgrades it to write). If only the
    // writedata question is observed, drop `readD` from the expected set.
    testCreateFunctionD: function () {
      dropFnD();
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/aqlfunction`, fnBody);
      assertPermissions([useD, readD, writeD], endObserve());
    },

    // DELETE /_db/d/_api/aqlfunction/{name} - WRITE trx (remove) over _aqlfunctions
    // AUDIT: see testCreateFunctionD - a directly-WRITE SingleCollectionTransaction
    // may emit only the writedata question rather than read+writedata.
    testDeleteFunctionD: function () {
      makeFnD();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/aqlfunction/${fnName}`);
      assertPermissions([useD, readD, writeD], endObserve());
    },

    // GET /_db/_system/_api/aqlfunction - AQL query over _aqlfunctions (read trx)
    testListFunctionsSystem: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/aqlfunction`);
      assertPermissions([useSystem, readSys], endObserve());
    },

    // GET /_db/_system/_api/aqlfunction/APITESTNS - same, filtered by namespace
    testListFunctionsByNamespaceSystem: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/aqlfunction/APITESTNS`);
      assertPermissions([useSystem, readSys], endObserve());
    },

    // POST /_db/_system/_api/aqlfunction - WRITE trx (insert) over _aqlfunctions
    // AUDIT: see testCreateFunctionD re read+writedata vs writedata-only.
    testCreateFunctionSystem: function () {
      dropFnSys();
      beginObserve();
      arango.POST_RAW(`/_db/_system/_api/aqlfunction`, fnBody);
      assertPermissions([useSystem, readSys, writeSys], endObserve());
    },

    // DELETE /_db/_system/_api/aqlfunction/{name} - WRITE trx (remove)
    // AUDIT: see testCreateFunctionD re read+writedata vs writedata-only.
    testDeleteFunctionSystem: function () {
      makeFnSys();
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/aqlfunction/${fnName}`);
      assertPermissions([useSystem, readSys, writeSys], endObserve());
    },
  };
}

jsunity.run(aqlFunctionApiAuthzSuite);
return jsunity.done();

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

// Authorization questions asked by the /_api/view endpoint family.
//
// Observation-based counterpart of tests/api/apitests/views.mjs.
//
// Handler: arangod/RestHandler/RestViewHandler.cpp
//
// Every request first asks `UseApiVersion version=0` and then
// `UseDatabase name=d level=read` in
// RestHandler::checkUserCanAccess(). The view handler then asks one dedicated
// ExecContext question per operation (arangod/Utils/ExecContext.cpp):
//   getViews (list)     -> canSeeView()   -> SeeView    (per visible view)
//   getView             -> canUseView(RO) -> UseView ... level=read
//   createView          -> canCreateView()-> CreateView ... linkedCollections=[..]
//   modifyView (props)  -> canModifyView()-> ModifyView ... linkedCollections=[..]
//   modifyView (rename) -> canRenameView()-> RenameView oldName=.. newName=..
//   deleteView          -> canDropView()  -> DropView
// The linkedCollections list is derived from the request body's `links` field.

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
  DOC_COLLECTION,
  singleOnly,
  clusterOnly
} = require('@arangodb/testutils/apitest-fixtures');

function viewApiAuthzSuite () {
  const c = DOC_COLLECTION;
  const TEST_VIEW = 'v_apitest';
  const TEST_VIEW_NEW = 'v_apitest_new';
  const viewBody = {
    name: TEST_VIEW,
    type: 'arangosearch',
    links: { c: { includeAllFields: true } }
  };

  // AUDIT: on a single server an arangosearch link is resolved twice - once by
  // the collection's name and once by its id, i.e. the id is passed where a
  // name is expected. A coordinator only resolves it by name. The id is the one
  // value that cannot be spelled out below; it is read in setUpAll, so that no
  // request is sent while an observation is running.
  let cId;

  function dropView (name) {
    arango.DELETE_RAW(`/_db/${DB}/_api/view/${name}`);
  }
  function createView () {
    dropView(TEST_VIEW);
    arango.POST_RAW(`/_db/${DB}/_api/view`, viewBody);
  }

  return {
    setUpAll: function () {
      setUpApiTestData();
      db._useDatabase(DB);
      cId = db._collection(c)._id;
      db._useDatabase('_system');
    },
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
      dropView(TEST_VIEW);
      dropView(TEST_VIEW_NEW);
    },

    // GET /_api/view - getViews() asks canSeeView() for every enumerable view
    // in the database. We create v_apitest so the list is non-empty, then
    // enumerate all views in d to build the expected SeeView set.
    testListViews: function () {
      createView();
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/view`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read",
        "SeeView db=d name=v_apitest",
        ...singleOnly([
          `UseCollection db=d name=${cId} level=read`
        ])
      ], endObserve());
    },

    // POST /_api/view - createView() -> canCreateView(linkedCollections=[c]).
    // Establishing the link resolves c, see linkedC().
    testCreateView: function () {
      dropView(TEST_VIEW);
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/view`, viewBody);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "CreateView db=d name=v_apitest linkedCollections=[c]",
        "UseCollection db=d name=c level=read",
        ...singleOnly([
          `UseCollection db=d name=${cId} level=read`,
          "UseCollection db=d name=c level=writedata"
        ]),
        ...clusterOnly([
          "UseCollection db=d name=c level=writemeta"
        ])
      ], endObserve());
    },

    // GET /_api/view/v_apitest - getView() -> canUseView(Read)
    testGetView: function () {
      createView();
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseView db=d name=v_apitest level=read",
        "UseCollection db=d name=c level=read",
        ...singleOnly([
          `UseCollection db=d name=${cId} level=read`
        ])
      ], endObserve());
    },

    // GET /_api/view/v_apitest/properties - getView(detailed) -> canUseView(Read)
    testGetViewProperties: function () {
      createView();
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/properties`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseView db=d name=v_apitest level=read",
        "UseCollection db=d name=c level=read",
        ...singleOnly([
          `UseCollection db=d name=${cId} level=read`
        ])
      ], endObserve());
    },

    // PATCH /_api/view/v_apitest/properties - modifyView() -> canModifyView().
    // The body ({cleanupIntervalStep:2}) has no `links` field, so
    // linkedCollections is empty.
    // The currently linked collection c is resolved on the way, see
    // linkedC().
    testUpdateViewProperties: function () {
      createView();
      beginObserve();
      arango.PATCH_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/properties`,
                       { cleanupIntervalStep: 2 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "ModifyView db=d name=v_apitest linkedCollections=[]",
        "UseCollection db=d name=c level=read",
        ...singleOnly([
          `UseCollection db=d name=${cId} level=read`
        ])
      ], endObserve());
    },

    // PUT /_api/view/v_apitest/properties - modifyView() -> canModifyView().
    // The body ({}) has no `links` field, so linkedCollections is empty.
    // Replacing the properties resets the links map, which resolves the
    // previously linked c, see linkedC().
    testReplaceViewProperties: function () {
      createView();
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/properties`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "ModifyView db=d name=v_apitest linkedCollections=[]",
        "UseCollection db=d name=c level=read",
        ...singleOnly([
          `UseCollection db=d name=${cId} level=read`
        ]),
        ...clusterOnly([
          "UseCollection db=d name=c level=writemeta",
          "UseDatabase name=d level=write"
        ])
      ], endObserve());
    },

    // PATCH /_api/view/v_apitest/rename - modifyView(rename) -> canRenameView()
    testRenameViewPatch: function () {
      createView();
      dropView(TEST_VIEW_NEW);
      beginObserve();
      arango.PATCH_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/rename`,
                       { name: TEST_VIEW_NEW });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "RenameView db=d oldName=v_apitest newName=v_apitest_new",
        "UseCollection db=d name=c level=read",
        ...singleOnly([
          `UseCollection db=d name=${cId} level=read`
        ])
      ], endObserve());
    },

    // PUT /_api/view/v_apitest/rename - modifyView(rename) -> canRenameView()
    testRenameViewPut: function () {
      createView();
      dropView(TEST_VIEW_NEW);
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/rename`,
                     { name: TEST_VIEW_NEW });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "RenameView db=d oldName=v_apitest newName=v_apitest_new",
        "UseCollection db=d name=c level=read",
        ...singleOnly([
          `UseCollection db=d name=${cId} level=read`
        ])
      ], endObserve());
    },

    // DELETE /_api/view/v_apitest - deleteView() -> canDropView()
    testDropView: function () {
      createView();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "DropView db=d name=v_apitest",
        ...clusterOnly([
          "UseDatabase name=d level=write",
          "UseCollection db=d name=c level=writemeta"
        ])
      ], endObserve());
    },
  };
}

jsunity.run(viewApiAuthzSuite);
return jsunity.done();

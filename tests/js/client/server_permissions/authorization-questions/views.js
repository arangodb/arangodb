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
// Every request first asks `UseDatabase name=d level=read` in
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
    'log.force-direct': 'true'
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
  DOC_COLLECTION
} = require('@arangodb/testutils/apitest-fixtures');

function viewApiAuthzSuite () {
  const useD = `UseDatabase name=${DB} level=read`;
  const c = DOC_COLLECTION;
  const TEST_VIEW = 'v_apitest';
  const TEST_VIEW_NEW = 'v_apitest_new';
  const viewBody = {
    name: TEST_VIEW,
    type: 'arangosearch',
    links: { c: { includeAllFields: true } }
  };

  function dropView (name) {
    arango.DELETE_RAW(`/_db/${DB}/_api/view/${name}`);
  }
  function createView () {
    dropView(TEST_VIEW);
    arango.POST_RAW(`/_db/${DB}/_api/view`, viewBody);
  }

  return {
    setUpAll: setUpApiTestData,
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
      db._useDatabase(DB);
      const names = db._views().map((v) => v.name());
      db._useDatabase('_system');
      const expected = [useD].concat(
        names.map((n) => `SeeView db=${DB} name=${n}`));
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/view`);
      assertPermissions(expected, endObserve());
    },

    // POST /_api/view - createView() -> canCreateView(linkedCollections=[c])
    // AUDIT: LogicalView::create for an arangosearch view with a link to c may
    //        ask additional collection questions (e.g. UseCollection on c)
    //        when establishing the link.
    testCreateView: function () {
      dropView(TEST_VIEW);
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/view`, viewBody);
      assertPermissions([useD,
                         `CreateView db=${DB} name=${TEST_VIEW} linkedCollections=[${c}]`],
                        endObserve());
    },

    // GET /_api/view/v_apitest - getView() -> canUseView(Read)
    testGetView: function () {
      createView();
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}`);
      assertPermissions([useD, `UseView db=${DB} name=${TEST_VIEW} level=read`],
                        endObserve());
    },

    // GET /_api/view/v_apitest/properties - getView(detailed) -> canUseView(Read)
    testGetViewProperties: function () {
      createView();
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/properties`);
      assertPermissions([useD, `UseView db=${DB} name=${TEST_VIEW} level=read`],
                        endObserve());
    },

    // PATCH /_api/view/v_apitest/properties - modifyView() -> canModifyView().
    // The body ({cleanupIntervalStep:2}) has no `links` field, so
    // linkedCollections is empty.
    // AUDIT: a partial property update may still touch the currently linked
    //        collection c; verify no extra UseCollection question appears.
    testUpdateViewProperties: function () {
      createView();
      beginObserve();
      arango.PATCH_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/properties`,
                       { cleanupIntervalStep: 2 });
      assertPermissions([useD,
                         `ModifyView db=${DB} name=${TEST_VIEW} linkedCollections=[]`],
                        endObserve());
    },

    // PUT /_api/view/v_apitest/properties - modifyView() -> canModifyView().
    // The body ({}) has no `links` field, so linkedCollections is empty.
    // AUDIT: replacing properties resets the links map (drops the link to c),
    //        which may add a UseCollection question for c.
    testReplaceViewProperties: function () {
      createView();
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/properties`, {});
      assertPermissions([useD,
                         `ModifyView db=${DB} name=${TEST_VIEW} linkedCollections=[]`],
                        endObserve());
    },

    // PATCH /_api/view/v_apitest/rename - modifyView(rename) -> canRenameView()
    testRenameViewPatch: function () {
      createView();
      dropView(TEST_VIEW_NEW);
      beginObserve();
      arango.PATCH_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/rename`,
                       { name: TEST_VIEW_NEW });
      assertPermissions([useD,
                         `RenameView db=${DB} oldName=${TEST_VIEW} newName=${TEST_VIEW_NEW}`],
                        endObserve());
    },

    // PUT /_api/view/v_apitest/rename - modifyView(rename) -> canRenameView()
    testRenameViewPut: function () {
      createView();
      dropView(TEST_VIEW_NEW);
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}/rename`,
                     { name: TEST_VIEW_NEW });
      assertPermissions([useD,
                         `RenameView db=${DB} oldName=${TEST_VIEW} newName=${TEST_VIEW_NEW}`],
                        endObserve());
    },

    // DELETE /_api/view/v_apitest - deleteView() -> canDropView()
    testDropView: function () {
      createView();
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/view/${TEST_VIEW}`);
      assertPermissions([useD, `DropView db=${DB} name=${TEST_VIEW}`],
                        endObserve());
    },
  };
}

jsunity.run(viewApiAuthzSuite);
return jsunity.done();

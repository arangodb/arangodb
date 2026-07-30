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

// Demonstrates the permissions observer: which authorization questions does a
// RestHandler ask the ExecContext? See
// js/client/modules/@arangodb/testutils/permissions-observer.js
//
// Authentication must be on, otherwise every context is AuthMode::Disabled and
// the handlers do not ask anything at all.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    // the observer reads the log file right after the request, so the log must
    // not be written by the logging thread
    'log.force-direct': 'true',
    // keep background threads from accessing collections while we observe
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

function authorizationQuestionsSuite () {
  const isCluster = require('internal').isCluster();
  const cn = 'UnitTestsAuthzQuestions';
  // every request checks read access to the database it addresses, see
  // RestHandler::checkUserCanAccess()
  const useSystem = 'UseDatabase name=_system level=read';

  return {
    setUp: function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown: function () {
      // in case a test failed in the middle of an observation
      disableObserve();
      db._drop(cn);
    },

    testCollectionCount: function () {
      beginObserve();
      arango.GET_RAW(`/_api/collection/${cn}/count`);
      assertPermissions([useSystem,
                         `UseCollection db=_system name=${cn} level=read`],
                        endObserve());
    },

    testInsertDocument: function () {
      let expected = [useSystem,
                      `UseCollection db=_system name=${cn} level=writedata`];
      if (!isCluster) {
        // the single server additionally asks for read access
        expected.push(`UseCollection db=_system name=${cn} level=read`);
      }
      beginObserve();
      arango.POST_RAW(`/_api/document/${cn}`, { value: 1 });
      assertPermissions(expected, endObserve());
    },

    // dropping a collection also cleans up graph definitions, hence the
    // question about _graphs
    testDropCollection: function () {
      let expected = [useSystem,
                      `DropCollection db=_system name=${cn}`,
                      `UseCollection db=_system name=${cn} level=read`,
                      'UseCollection db=_system name=_graphs level=read'];
      if (!isCluster) {
        // the single server also revokes the collection's permissions from all
        // users; in the cluster that happens without an ExecContext
        expected.push('UseCollection db=_system name=_users level=read');
      }
      beginObserve();
      arango.DELETE_RAW(`/_api/collection/${cn}`);
      assertPermissions(expected, endObserve());
    },

    // a handler that asks nothing beyond the database access every request
    // checks - proves the observer does not pick up ambient traffic
    testVersion: function () {
      beginObserve();
      arango.GET_RAW('/_api/version');
      assertPermissions([useSystem], endObserve());
    },
  };
}

jsunity.run(authorizationQuestionsSuite);
return jsunity.done();

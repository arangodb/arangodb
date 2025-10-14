/* jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global assertEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
// /
// / @author Michael Hackstein
// / @author Copyright 2018, ArangoDB Inc., Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const jsunity = require('jsunity');
let IM = global.instanceManager;

const request = require('@arangodb/request');

const db = require("@arangodb").db;

const MOUNT = "/test";
const serviceUrl = (url) => {
  return url + "/_db/" + encodeURIComponent(db._name()) + MOUNT;
};
const _ = require('lodash');
const options = {
  json: true
};

function getCoordinators() {
  return IM.arangods.filter(arangod => {
    return arangod.isFrontend(); }).map(arangod => {
      return arangod.url;}).map(serviceUrl);
}


function foxxTestSuite () {
  return {
    setUp: () => {
    },
    tearDown: () => {},

    testServiceIsMounted: function () {
      const serversToTest = getCoordinators();
      const res = request.get(serversToTest[0], options);
      assertEqual(200, res.statusCode);
      assertEqual({hello: 'world'}, res.json);
    },

    testServiceIsPropagated: function () {
      const serversToTest = getCoordinators();
      serversToTest.forEach(m => {
        const res = request.get(m, options);
        assertEqual(200, res.statusCode);
        assertEqual({hello: 'world'}, res.json);
      });
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////
if (!IM.options.skipServerJS) {
  jsunity.run(foxxTestSuite);
}
return jsunity.done();


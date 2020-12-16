/* jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global assertEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for dump/reload
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2018-2018 ArangoDB Inc., Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB Inc., Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2018, ArangoDB Inc., Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const jsunity = require('jsunity');

const request = require('@arangodb/request');
const endpointToURL = (endpoint) => {
  if (endpoint.substr(0, 6) === 'ssl://') {
    return 'https://' + endpoint.substr(6);
  }
  const pos = endpoint.indexOf('://');
  if (pos === -1) {
    return 'http://' + endpoint;
  }
  return 'http' + endpoint.substr(pos);
};

const db = require("@arangodb").db;

const MOUNT = "/test";
const serviceUrl = (endpoint) => {
  return endpointToURL(endpoint) + "/_db/" + encodeURIComponent(db._name()) + MOUNT;
};
const baseUrl = serviceUrl(arango.getEndpoint());
const _ = require('lodash');
const options = {
  json: true
};

require("@arangodb/test-helper").waitForFoxxInitialized();

function getCoordinators() {
  const isCoordinator = (d) => (_.toLower(d.role) === 'coordinator');
  const toEndpoint = (d) => (d.endpoint);
  const instanceInfo = JSON.parse(require('internal').env.INSTANCEINFO);
  return instanceInfo.arangods.filter(isCoordinator)
                              .map(toEndpoint)
                              .map(serviceUrl);
}


function foxxTestSuite () {
  return {
    setUp: () => {
    },
    tearDown: () => {},

    testServiceIsMounted: function () {
      const res = request.get(baseUrl, options);
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

jsunity.run(foxxTestSuite);

return jsunity.done();


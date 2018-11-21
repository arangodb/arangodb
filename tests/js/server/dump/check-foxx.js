/* jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global assertEqual */

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
const MOUNT = '/test';
const _ = require('lodash');
const options = {
  json: true
};

function getCoordinators() {
  const isCoordinator = (d) => (_.toLower(d.role) === 'coordinator');
  const toEndpoint = (d) => (d.endpoint);
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

  const instanceInfo = JSON.parse(require('internal').env.INSTANCEINFO);
  return instanceInfo.arangods.filter(isCoordinator)
                              .map(toEndpoint)
                              .map(endpointToURL);
}


function foxxTestSuite () {
  return {
    setUp: () => {},
    tearDown: () => {},

    testServiceIsMounted: function () {
      const res = request.get(MOUNT, options);
      assertEqual(200, res.statusCode);
      assertEqual({hello: 'world'}, res.json);
    },

    testServiceIsPropagated: function () {
      const serversToTest = getCoordinators();
      require("internal").print(serversToTest);
      /*
      if (isCluster) {
        // TODO Test both coordinators.
      } else {
        assertFalse(isCluster);
      }
      */
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(foxxTestSuite);

return jsunity.done();


/* jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global */

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

const jsunity = require('jsunity');
const {assertEqual} = jsunity.jsUnity.assertions;
const arango = require("@arangodb").arango;
const db = require("@arangodb").db;
let IM = global.instanceManager;

const path = "/_db/" + encodeURIComponent(db._name()) + "/test";
const _ = require('lodash');


function foxxTestSuite () {
  return {
    setUp: () => {
      IM.rememberConnection();
    },
    tearDown: () => {
      IM.reconnectMe();
    },

    testServiceIsMounted: function () {
      IM.rememberConnection();
      IM.arangods.forEach(d => {
        if (d.isFrontend()){
          d.toThisInstance(() => {
            let res = arango.GET_RAW(path);
            assertEqual(200, res.code);
            assertEqual({hello: 'world'}, res.parsedBody);
          });
          return;
        }
      });
      IM.reconnectMe();
    },

    testServiceIsPropagated: function () {
      IM.rememberConnection();
      IM.arangods.forEach(d => {
        if (d.isFrontend()){
          d.toThisInstance(() => {
            let res = arango.GET_RAW(path);
            assertEqual(200, res.code);
            assertEqual({hello: 'world'}, res.parsedBody);
          });
        }
      });
      IM.reconnectMe();
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


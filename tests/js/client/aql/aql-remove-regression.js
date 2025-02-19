/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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
/// @author Markus Pfeiffer
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const isEnterprise = require("internal").isEnterprise();
const errors = internal.errors;
const db = require("@arangodb").db;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;

function removeRegressionSuite() {
  const cname = "UnitTestCollectionFoo";

  return {
    setUpAll : function () {
      db._create(cname);
    },
    tearDownAll : function () {
      try {
        db._drop(cname);
      } catch(e) {}
    },
    testRegressionCrashEmptyRemove : function () {
      const query = `FOR u IN ${cname}
                       REMOVE u IN ${cname}
                       LET removed = { _key: "UnitTestCollection/",
                                       value: 1,
                                       sortValue: 1,
                                       nestedObject: {
                                         "name": u.name,
                                         "isActive": u.status == "active"
                                       }
                                     }
                       RETURN removed.t.UnitTestCollection`;
      let actual = db._query(query);
      assertEqual(actual.toArray().length, 0);
    },
    testRegressionCrashEmptyRemoveSmaller : function () {
      const query = `FOR u IN ${cname}
                       LET removed = { nestedVariableUse: u.name }
                       RETURN removed.t.UnitTestCollection`;
      let actual = db._query(query);
      assertEqual(actual.toArray().length, 0);
    },

  };
}

jsunity.run(removeRegressionSuite);
return jsunity.done();

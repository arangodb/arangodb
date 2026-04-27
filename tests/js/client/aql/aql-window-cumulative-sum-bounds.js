/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, fail */

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
/// @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const errors = internal.errors;
const isCluster = internal.isCluster();
const isEnterprise = internal.isEnterprise();

const collectionName = "UnitTestWindowCollection";

const base = require("fs").join(require('internal').pathForTesting('client'),
             'aql', 'aql-window-cumulative-sum.inc');
const generateWindowBoundsSuite = require("internal").load(base);

function WindowBoundsSuite() {
  const suite = generateWindowBoundsSuite("SingleShard");
  suite.setUpAll = function() {
    db._create(collectionName, {numberOfShards: 1});
    let docs = [];
    for (let i = 0; i < 10; ++i) {
      docs.push({key: `${i}`,
        v: i,
        time: `2021-05-25 07:${i.toString().padStart(2, '0')}:00`
      });
    }
    db._collection(collectionName).save(docs);
  };
  suite.tearDownAll = function() {
    db._drop(collectionName);
  };
  return suite;
}

jsunity.run(WindowBoundsSuite);
return jsunity.done();

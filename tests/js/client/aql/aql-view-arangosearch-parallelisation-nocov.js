/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

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
/// @author Andrei Lobov
/// @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const isCluster = require("internal").isCluster();
const analyzers = require("@arangodb/analyzers");
var internal = require("internal");
let db = require("@arangodb").db;
let IM = global.instanceManager;

function ArangoSearchParallelisationTestSuite () {
    return {
    setUpAll : function () {
        IM.debugClearFailAt();
        analyzers.save("mygeojson", "geojson", {type:"shape"});
        let c = db._create("UnitTestCollection");
        db._createView("UnitTestView", "arangosearch",
          { 
            consolidationIntervalMasec:0,
            storedValues: ["gg"],
            primarySort: [{field:"foo", direction:"asc"}],
            links: {
              UnitTestCollection:{
                fields:{
                  foo:{analyzers:["identity"]},
                  gg:{analyzers:["mygeojson"]}}}}});
                  
        let data =[];
        for (let i = 0; i < 1002; ++i) {
            data.push({foo: i, gg: { "type": "Point", "coordinates": [ 37.701645, 55.832144 ] }});
        }
        c.save(data);
        db._query("FOR d IN UnitTestView OPTIONS {waitForSync:true} LIMIT 1 RETURN d");
        data = [];
        for (let i = 0; i < 500; ++i) {
            data.push({foo: "f" + i, gg: { "type": "Point", "coordinates": [ 47.701645, 55.832144 ] }});
        }
        c.save(data);
        db._query("FOR d IN UnitTestView OPTIONS {waitForSync:true} LIMIT 1 RETURN d");
        data = [];
        for (let i = 0; i < 1000; ++i) {
            data.push({foo: "d" + i, gg: { "type": "Point", "coordinates": [ 57.701645, 55.832144 ] }});
        }
        c.save(data);
        db._query("FOR d IN UnitTestView OPTIONS {waitForSync:true} LIMIT 1 RETURN d");
        data = [];
        for (let i = 0; i < 2000; ++i) {
            data.push({foo: "e" + i, gg: { "type": "Point", "coordinates": [ 27.701645, 55.832144 ] }});
        }
        c.save(data);
        data = [];
        db._query("FOR d IN UnitTestView OPTIONS {waitForSync:true} LIMIT 1 RETURN d");
        IM.debugSetFailAt("IResearchFeature::failNonParallelQuery");
    },
    tearDownAll : function() {
        IM.debugClearFailAt();
        db._dropView("UnitTestView");
        db._drop("UnitTestCollection");
        analyzers.remove("mygeojson", true);
    },
    testFullIteration : function() {
        let res = db._query(
        "LET go = GEO_POLYGON([[[0, 0], [58, 0],[58, 58], [0, 58], [0, 0]]]) " +
        "FOR d IN UnitTestView SEARCH " +
        " ANALYZER(GEO_INTERSECTS(go, d.gg), 'mygeojson') " +
        " OPTIONS {parallelism:32} " +
        " RETURN d");
        assertEqual(res.toArray().length, 4502);
    },
    testFullIterationOffset : function() {
        let res = db._query(
        "LET go = GEO_POLYGON([[[0, 0], [58, 0],[58, 58], [0, 58], [0, 0]]]) " +
        "FOR d IN UnitTestView SEARCH " +
        " ANALYZER(GEO_INTERSECTS(go, d.gg), 'mygeojson') " +
        " OPTIONS {parallelism:32} " +
        " LIMIT 10, 3000" +  
        " RETURN d");
        assertEqual(res.toArray().length, 3000);
    },
    testFullIterationLimitFullCount : function() {
        let res = db._query(
        "LET go = GEO_POLYGON([[[0, 0], [58, 0],[58, 58], [0, 58], [0, 0]]]) " +
        "FOR d IN UnitTestView SEARCH " +
        " ANALYZER(GEO_INTERSECTS(go, d.gg), 'mygeojson') " +
        " OPTIONS {parallelism:32} " +
        " LIMIT 10, 3999" +  
        " RETURN d", {}, {fullCount:true});
        assertEqual(4502, res.getExtra().stats.fullCount);
        assertEqual(4502, res.getExtra().stats.scannedIndex);
        assertEqual(res.toArray().length, 3999);
    }
  };
}

jsunity.run(ArangoSearchParallelisationTestSuite);

return jsunity.done();

/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db, indexId;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////


const activateSplicing = {optimizer: {rules: ["+splice-subqueries"]}};
const deactivateSplicing = {optimizer: {rules: ["-splice-subqueries"]}};


const deepAssertElements = (left, right, path) => {
  if (Array.isArray(left)) {
    assertTrue(Array.isArray(right), `On ${path}`);
    assertEqual(left.length, right.length, `On ${path}`);
    for (let i = 0; i < left.length; ++i) {
      deepAssertElements(left[i], right[i], `${path}[${i}]`);
    }
  } else if (left instanceof Object) {
    assertTrue(right instanceof Object, `On ${path}`);
    assertEqual(Object.keys(left).sort(), Object.keys(right).sort(), `On ${path}`);
    for (const [key, val] of Object.entries(left)) {
      deepAssertElements(val, right[key], `${path}.${key}`);
    }
  } else {
    assertEqual(left, right,`On ${path}`);
  }
};

function blockReturnRegressionSuite() {


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reuse of blocks in subquery, they are of identical size each time
////////////////////////////////////////////////////////////////////////////////

    testBlockReuseOkWithSubquery : function () {
      const query = `FOR c in 1..1000 LET su = (FOR d in 1..1000 SORT d RETURN d) RETURN LENGTH(su)`;
      var actual = db._query(query);
      assertEqual(actual.toArray().length , 1000);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief generated subquery, tends to show a difference 
////////////////////////////////////////////////////////////////////////////////

    testHeavyNestedSubquery : function () {
      
      const query = `
      FOR fv0 IN 1..20
        LET sq1 = (FOR fv2 IN 1..20
          LIMIT 10,0
          COLLECT WITH COUNT INTO counter 
          RETURN {counter})
        LET sq3 = (FOR fv4 IN 1..20
          LET sq5 = (FOR fv6 IN 1..20
            LET sq7 = (FOR fv8 IN 1..20
              LET sq9 = (FOR fv10 IN 1..20
                LIMIT 4,7
                COLLECT WITH COUNT INTO counter 
                RETURN {counter})
              LIMIT 11,11
              RETURN {fv8, sq1, sq9})
            LET sq11 = (FOR fv12 IN 1..20
              LIMIT 19,17
              RETURN {fv12, sq1, sq7})
            LIMIT 12,5
            RETURN {fv6, sq1, sq7, sq11})
          LIMIT 11,5
          RETURN {fv4, sq1, sq5})
        LIMIT 10,11
        RETURN {fv0, sq1, sq3}
      `;

      const splicedRes = db._query(query, {}, activateSplicing).toArray();
      const nosplicedRes = db._query(query, {}, deactivateSplicing).toArray();
      deepAssertElements(splicedRes, nosplicedRes, "result");
    },
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief generated subquery, tends to show a difference 
    ////////////////////////////////////////////////////////////////////////////////

    testHeavyNestedSubquery2 : function () {
      const query = `  
        FOR fv62 IN 1..100
        LET sq63 = (FOR fv64 IN 1..100
          LET sq65 = (FOR fv66 IN 1..100
            LET sq67 = (FOR fv68 IN 1..100
                        LIMIT 11,19
                        RETURN {fv68})
            LET sq69 = (FOR fv70 IN 1..100
              LET sq71 = (FOR fv72 IN 1..100
                          LIMIT 12,10
                          RETURN {fv72, sq67})
                       FILTER fv70 < 7
                       LIMIT 11,0
                       RETURN {fv70, sq67, sq71})
                LIMIT 2,6
                RETURN {fv66, sq67, sq69})
          LET sq73 = (FOR fv74 IN 1..100
                      LIMIT 15,15
                      RETURN {fv74, sq65})
          FILTER fv62 < 13
          LIMIT 4,13
          COLLECT WITH COUNT INTO counter
          RETURN {counter})
        LIMIT 6,19
        RETURN {fv62, sq63}
      `;
      const splicedRes = db._query(query, {}, activateSplicing).toArray();
      const nosplicedRes = db._query(query, {}, deactivateSplicing).toArray();
      deepAssertElements(splicedRes, nosplicedRes, "result");
    },

    testEmptyCollections: function () {
      const col = db._createEdgeCollection("UnitTestCollection", {numberOfShards: 5});
      try {
        const query = `
        FOR x IN @@collectionName
          LET sq = (FOR y IN @@collectionName LIMIT 10 RETURN UNSET(y, "_rev"))
          LET sq2 = (FOR z IN @@collectionName LIMIT 10 RETURN UNSET(z, "_rev"))
            RETURN [sq, sq2]
        `;
        const bindVars = { "@collectionName": col.name() };
        const splicedRes = db._query(query, bindVars, activateSplicing).toArray();
        const nosplicedRes = db._query(query, bindVars, deactivateSplicing).toArray();
        deepAssertElements(splicedRes, nosplicedRes, "result");
      } finally {
        col.drop();
      }
    }
  };

}

function collectionInSubqeryRegressionSuite() {
  let col;

  return {
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      col = db._createEdgeCollection("UnitTestCollection", { numberOfShards: 5 });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      col.drop();
    },


    testEmptyCollections2: function () {
      const query = `
      FOR x IN @@collectionName
        LET sq = (FOR y IN @@collectionName LIMIT 10 RETURN UNSET(y, "_rev"))
        LET sq2 = (FOR z IN @@collectionName LIMIT 10 RETURN UNSET(z, "_rev"))
          RETURN [sq, sq2]
      `;
      const bindVars = { "@collectionName": col.name() };
      const splicedRes = db._query(query, bindVars, activateSplicing).toArray();
      const nosplicedRes = db._query(query, bindVars, deactivateSplicing).toArray();
      deepAssertElements(splicedRes, nosplicedRes, "result");
    },

    testInsertCollections: function () {
      // We do not expect any inserts.
      // However as opposed to the query in testEmptyCollections, we will use a Distribute
      // instead of a Scatter here, using a different code path
      const query = `
      FOR x IN 1..10
        LIMIT 100, null
        LET sq2 = (INSERT {value: x._key} INTO @@collectionName)
          RETURN [sq2]
      `;
      const bindVars = { "@collectionName": col.name() };
      const splicedRes = db._query(query, bindVars, activateSplicing).toArray();
      const nosplicedRes = db._query(query, bindVars, deactivateSplicing).toArray();
      deepAssertElements(splicedRes, nosplicedRes, "result");
    },

    testParallelTraversal: function () {
      // We do not expect any traversal
      // however this should trigger the distribution
      // into parallel threads.
      const query = `
      FOR x IN @@collectionName
        LET sq = (FOR y IN @@collectionName LIMIT 10 RETURN UNSET(y, "_rev"))
        LET sq2 = (FOR v,e,p IN 1 OUTBOUND x @@collectionName OPTIONS {parallelism: 5} RETURN v)
          RETURN [sq, sq2]
      `;
      const bindVars = { "@collectionName": col.name() };
      const splicedRes = db._query(query, bindVars, activateSplicing).toArray();
      const nosplicedRes = db._query(query, bindVars, deactivateSplicing).toArray();
      deepAssertElements(splicedRes, nosplicedRes, "result");
    }
  };
}

function nonMaterializingViewRegressionSuite() {
  const cname = "UnitTestCollection";
  const vname = "UnitTestView";

  const cleanup = () => {
    try {
      db._drop(cname);
    } catch(e) {}
    try {
      db._dropView(vname);
    } catch(e) {}
  };

  return {
    setUp: function () {
      cleanup();
      db._create(cname);
      // create a view on the collection, specifics here do not matter
      db._createView(vname, "arangosearch", {links:{[cname]:{includeAllFields:true}}});
      // We need more docs then batchSize, to have enough rows available in the view
      const docs = [];
      for (let i = 0; i < 1010; ++i) {
        docs.push({});
      }
      db[cname].save(docs);
    },

    tearDown: function () {
      cleanup();
    },

    testRowsWithoutRegisters: function() {
      // This tests a logic assertion in the ExecutionBlock.
      // If a Block (view node) HASMORE it is required to write something
      // In this Query `d` is optimized away, so the Block will write
      // empty rows only.
      // This test requires that the Producer (View) can optimize away its output
      // and the consumer (Subquery) cannot passthrough the Block.
      // Hence all optimzation is turned off to guarantee these two stay connected.
      const query = `FOR d IN ${vname} OPTIONS {waitForSync: true} LET p = (RETURN 1 + 1) RETURN 1`;
      const res = db._query(query, {}, {optimizer: {rules: ["-all", "+splice-subqueries"] }});
      // This result was never wrong, the internal assertion is tested. 
      assertEqual(res.toArray().length, 1010);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(blockReturnRegressionSuite);
jsunity.run(collectionInSubqeryRegressionSuite);
jsunity.run(nonMaterializingViewRegressionSuite);

return jsunity.done();


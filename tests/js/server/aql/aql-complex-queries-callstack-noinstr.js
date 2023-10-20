/*jshint globalstrict:false, strict:false */
/*global assertEqual*/

let db = require("@arangodb").db;
let jsunity = require("jsunity");
const isCluster = require("@arangodb/cluster").isCluster();
const isEnterprise = require("internal").isEnterprise();

// The numbers hardcoded in the "cnt" values come from AQL not running properly
// because of too many nodes or too much nesting in the queries, so the values
// are around the limit for which the query would execute without internal AQL
// error. The tests seem strange because they were made with the goal of
// producing many nodes in the execution plan, without optimizations.
// For some of the queries, they would crash the server without the default
// value for the "maxNodesPerCallstack" query option. If a number bigger than the
// default value for this option is provided, some of the queries below would
// crash the server when running.

function ComplexQueriesTestSuite() {
  // for the following tests, set the memory limit to 0 for the tests not to return with the error that the query is using more memory than allowed for certain mac machines
  const collectionName = "testCollection";
  return {
    setUpAll: function() {
      db._create(collectionName);
      for (let i = 0; i < 50; ++i) {
        db[collectionName].insert({"name": "test" + i});
      }
    },

    tearDownAll: function() {
      db._drop(collectionName);
    },

    testLargeQuery1: function() {
      let q = "LET v0 = NOOPT(1)";
      const cnt = 3500;
      for (let i = 1; i < cnt; ++i) {
        q += ` LET v${i} = NOOPT(v${i - 1} + 1)\n`;
      }
      q += ` RETURN v${cnt - 1}`;
      const res = db._query(q, {}, {memoryLimit: 0}).toArray();
      assertEqual(res[0], cnt);
    },

    testLargeQuery2: function() {
      let q = "";
      const cnt = 500;
      for (let i = 1; i < cnt; ++i) {
        q += ` LET mySub${i} = (FOR doc in ${collectionName} FILTER CHECK_DOCUMENT(doc) RETURN doc.name)`;
        q += ` LET v${i} = 1`;
      }
      q += ` RETURN v${cnt - 1}`;
      const res = db._query(q, {}, {memoryLimit: 0}).toArray();
      assertEqual(res[0], 1);
    },

    testLargeQuery3: function() { //this crashes without default maxNodesPerCallstack
      let q = "LET v0 = NOOPT(1)";
      const cnt = 3900;
      for (let i = 1; i < cnt; ++i) {
        q += ` LET v${i} = NOOPT(${i})`;
      }
      q += ` RETURN v${cnt - 1}`;
      const res = db._query(q, {}, {memoryLimit: 0}).toArray();
      assertEqual(res[0], cnt - 1);
    },

    testLargeQuery4: function() {
      let q = "";
      const cnt = 250;
      for (let i = 1; i < cnt; ++i) {
        q += ` LET mySub${i} = (FOR doc in ${collectionName} FILTER CHECK_DOCUMENT(doc) FILTER doc.name == "test50"
                RETURN doc.name)`;
        q += ` LET v${i} = 1`;
      }
      q += ` RETURN mySub${cnt - 1}`;
      const res = db._query(q, {}, {memoryLimit: 0}).toArray();
      assertEqual(res[0], []);
    },

    testLargeQuery5: function() {
      let q = "";
      const cnt = 390;
      for (let i = 1; i < cnt; ++i) {
        q += ` LET mySub${i} = (FOR doc in ${collectionName} FILTER CHECK_DOCUMENT(doc) FILTER doc.name == "test1"
                RETURN doc.name)`;
      }
      q += ` RETURN mySub${cnt - 1}`;
      const res = db._query(q, {}, {memoryLimit: 0}).toArray();
      assertEqual(res[0], ["test1"]);
    },
  };
}

function ComplexQueriesSmartGraphTestSuite() {
  // for the following tests, set the memory limit to 0 for the tests not to return with the error that the query is using more memory than allowed for certain mac machines
  let smartGraphs = require("@arangodb/smart-graph");
  const edges = "SmartEdges";
  const vertex = "SmartVertices";
  const graph = "TestSmartGraph";
  return {
    setUpAll: function() {
      smartGraphs._create(graph, [smartGraphs._relation(edges, vertex, vertex)], null, {
        numberOfShards: 3,
        smartGraphAttribute: "testi"
      });

      for (let i = 0; i < 500; ++i) {
        db[vertex].insert({_key: "test" + (i % 10) + ":test" + i, testi: "test" + (i % 10)});
      }
      for (let i = 0; i < 500; ++i) {
        db[edges].insert({
          _from: vertex + "/test" + i + ":test" + (i % 10),
          _to: vertex + "/test" + ((i + 1) % 100) + ":test" + (i % 10),
          testi: (i % 10)
        });
      }
    },

    tearDownAll: function() {
      smartGraphs._drop(graph, true);
    },

    testSmartQuery1: function() {
      let q = `WITH ${vertex}`;
      const cnt = 500;
      for (let i = 1; i < cnt; ++i) {
        q += ` LET mySub${i} = (FOR v, e in 1..10 OUTBOUND "${vertex}/test0:test0" ${edges} FILTER CHECK_DOCUMENT(e)
                RETURN e.testi)`;
      }
      q += ` RETURN mySub${cnt - 1}`;
      const res = db._query(q, {}, {memoryLimit: 0}).toArray();
      assertEqual(res[0], [0]);
    },

    testSmartQuery2: function() {
      let q = `WITH ${vertex}`;
      const cnt = 100;
      for (let i = 1; i < cnt; ++i) {
        q += ` LET mySub${i} = (FOR v1, e1 in 1..10 OUTBOUND "${vertex}/test0:test0" ${edges} FOR v2, e2 in 1..10
                OUTBOUND e1._from ${edges} FILTER CHECK_DOCUMENT(e1) RETURN e1.testi)`;
      }
      q += ` RETURN mySub${cnt - 1}`;
      const res = db._query(q, {}, {memoryLimit: 0}).toArray();
      assertEqual(res[0], [0]);
    },

  };
}

jsunity.run(ComplexQueriesTestSuite);
if (isCluster && isEnterprise) {
  jsunity.run(ComplexQueriesSmartGraphTestSuite);
}
return jsunity.done();

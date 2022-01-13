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


function testLargeQuery4() {
  let q = "NOOPT(1)";
  const cnt = 3800;
  for (let i = 1; i < cnt; ++i) {
    q += `NOOPT(${i})`;
  }
  q += ` RETURN NOOPT(${cnt} - 1)`;
  return q;
}

function testLargeQuery5() {
  let q = "";
  const cnt = 200;
  for (let i = 1; i < cnt; ++i) {
    q +=  ` LET mySub${i} = (FOR doc in testCollection FILTER CHECK_DOCUMENT(doc) FILTER doc.name == "test50" RETURN doc.name)`;
    q += ` LET v${i} = 1`;
  }
  q += ` RETURN v${cnt - 1}`;
  return q;
}

function ComplexQueriesTestSuite() {

  return {
    setUpAll: function () {
      db._create("testCollection");
      for (let i = 0; i < 50; ++i) {
        db.testCollection.insert({"name": "test" + i, "age": i});
      }
    },

    tearDownAll: function () {
      db._drop("testCollection");
    },

    testLargeQuery1: function () {
      let q = "LET v0 = NOOPT(1)";
      const cnt = 3500;
      for (let i = 1; i < cnt; ++i) {
        q += ` LET v${i} = NOOPT(v${i - 1} + 1)\n`;
      }
      q += ` RETURN v${cnt - 1}`;
      const res = db._query(q, {}, {}).toArray();
      assertEqual(res[0], cnt);
    },

    testLargeQuery2: function () {
      let q = "";
      const cnt = 500;
      for (let i = 1; i < cnt; ++i) {
        q +=  ` LET mySub${i} = (FOR doc in testCollection FILTER CHECK_DOCUMENT(doc) RETURN doc.name)`;
        q += ` LET v${i} = 1`;
      }
      q += ` RETURN v${cnt - 1}`;
      const res = db._query(q).toArray();
      assertEqual(res[0], 1);
    },

    testLargeQuery3: function() { //this crashes without default maxNodesPerCallstack
      let q = "LET v0 = NOOPT(1)";
      const cnt = 3900;
      for (let i = 1; i < cnt; ++i) {
        q += ` LET v${i} = NOOPT(${i})`;
      }
      q += ` RETURN v${cnt - 1}`;
      const res = db._query(q, {}, {}).toArray();
      assertEqual(res[0], cnt -1);
    },
    testLargeQuery4: function() {

    }
  };
}

function ComplexQueriesSmartGraphTestSuite() {
  let smartGraphs = require("@arangodb/smart-graph");
  const edges = "SmartEdges";
  const vertex = "SmartVertices";
  const graph = "TestSmartGraph";
  return {
    setUpAll: function () {
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

    tearDownAll: function () {
      smartGraphs._drop(graph, true);
    },
    testSmartQuery1: function () {
      let q = "WITH SmartVertices";
      const cnt = 500;
      for (let i = 1; i < cnt; ++i) {
        q +=  ` LET mySub${i} = (FOR v, e in 1..10 OUTBOUND "SmartVertices/test0:test0" SmartEdges FILTER CHECK_DOCUMENT(e) RETURN e.testi)`;
      }
      q += ` RETURN mySub${cnt - 1}`;
      const res = db._query(q).toArray();
      assertEqual(res[0], [0]);
    },
    testSmartQuery2: function () {
      let q = "WITH SmartVertices";
      const cnt = 100;
      for (let i = 1; i < cnt; ++i) {
        q +=  ` LET mySub${i} = (FOR v1, e1 in 1..10 OUTBOUND "SmartVertices/test0:test0" SmartEdges FOR v2, e2 in 1..10 
                              OUTBOUND e1._from SmartEdges FILTER CHECK_DOCUMENT(e1) RETURN e1.testi)`;
      }
      q += ` RETURN mySub${cnt - 1}`;
      const res = db._query(q).toArray();
      assertEqual(res[0], [0]);
    },

  };
}

jsunity.run(ComplexQueriesTestSuite);
if (isCluster && isEnterprise) {
  jsunity.run(ComplexQueriesSmartGraphTestSuite);
}
return jsunity.done();

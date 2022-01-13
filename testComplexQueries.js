/*jshint globalstrict:false, strict:false */

function testLargeQuery1() {
  let q = "LET v0 = NOOPT(1)";
  const cnt = 3500;
  for (let i = 1; i < cnt; ++i) {
    q += ` LET v${i} = NOOPT(v${i - 1} + 1)\n`;
  }
  q += ` RETURN v${cnt - 1}`;
  return q;
}

function testLargeQuery2() {
  let q = "";
  const cnt = 500;
  for (let i = 1; i < cnt; ++i) {
    q +=  ` LET mySub${i} = (FOR doc in testCollection FILTER CHECK_DOCUMENT(doc) RETURN doc.name)`;
    q += ` LET v${i} = 1`;
  }
  q += ` RETURN v${cnt - 1}`;
  return q;
}

function testLargeQuery3() {
  let q = "LET v0 = NOOPT(1)";
  const cnt = 3800;
  for (let i = 1; i < cnt; ++i) {
    q += ` LET v${i} = NOOPT(${i})`
  }
  q += ` RETURN v${cnt - 1}`;
  return q;
}

function testLargeQuery4() {
  let q = "NOOPT(1)";
  const cnt = 3800;
  for (let i = 1; i < cnt; ++i) {
    q += `NOOPT(${i})`
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

function testSmartQuery1() {
  let q = "";
  const cnt = 200;
  for (let i = 1; i < cnt; ++i) {
    q +=  ` LET mySub${i} = (FOR e in _to_SmartEdges FILTER CHECK_DOCUMENT(e) RETURN e.testi)`;
  }
  q += ` RETURN mySub${i - 1}[10]`;
  return q;
}

function testSmartQuery2() {
  let q = "";
  const cnt = 200;
  for (let i = 1; i < cnt; ++i) {
    q +=  ` LET mySub${i} = (FOR e in _to_SmartEdges FILTER CHECK_DOCUMENT(e) FILTER e.id == "SmartEdges/test413:14024132:test14" RETURN e.testi)`;
  }
  q += ` RETURN mySub${i - 1}`;
  return q;
}


function testSmartQuery2() {
  let q = "";
  const cnt = 200;
  for (let i = 1; i < cnt; ++i) {
    q +=  ` LET mySub${i} = (FOR e in _to_SmartEdges LET myOtherSub = (FOR v in SmartVertices FILTER e._to == v.id RETURN v) RETURN e.testi)`;
  }
  q += ` RETURN mySub${i - 1}`;
  return q;
}

function TestQueries() {
  let db = require('@arangodb').db;

  try {
    db._createDatabase("testDatabase");
    db._useDatabase("testDatabase");

    db._create("testCollection");
    let docs = [];
    for (i = 0; i < 50; ++i) {
      db.testCollection.insert({"name": "test" + i, "age": i});
    }
    
    const q1 = testLargeQuery1();
    const q2 = testLargeQuery2();
    const q3 = testLargeQuery3();
    const q4 = testLargeQuery4();
    const q5 = testLargeQuery5();

    console.log(db._query(q2, {}, {maxNodesPerCallstack: 200000}).toArray()); //this doesn't crash with large stack

    console.log(db._query(q3, {}, {}).toArray()); //default max number of nodes per stack 250 or it will crash

    console.log(db._query(q5, {}, {}).toArray());


    const res = arango.GET("/_admin/server/role");
    if (res.role === "COORDINATOR") {
      const edges = "SmartEdges";
      const vertex = "SmartVerties";
      const graph = "TestSmartGraph";
      let smartGraphs = require("@arangodb/smart-graph");
      smartGraphs._create(graph, [smartGraphs._relation(edges, vertex, vertex)], null, { numberOfShards: 3, smartGraphAttribute: "testi" });

      for (let i = 0; i < 500; ++i) {
        db[vertex].insert({ _key: "test" + (i % 10) + ":test" + i, testi: "test" + (i % 10) });
      }
      for (let i = 0; i < 500; ++i) {
        db[edges].insert({ _from: vertex + "/test" + i + ":test" + (i % 10), _to: vertex + "/test" + ((i + 1) % 100) + ":test" + (i % 10), testi: (i % 10) });
      }
      db[edges].insert({ _from: vertex + "/test0:test0", _to: vertex + "/testmann-does-not-exist:test0", testi: "test0" });
    }
    qS1 = testSmartQuery1();
    console.log(db._query(qS1, {}, {}).toArray());
    console.log(db._query(qS1, {}, { optimizer: { rules: [ "-all" ] } }).toArray());
    qS2 = testSmartQuery1();
    console.log(db._query(qS2, {}, {}).toArray());
    console.log(db._query(qS2, {}, { optimizer: { rules: [ "-all" ] } }).toArray());

  } catch(error) {
    print(error);
  } finally {
    db._useDatabase("_system");
    db._dropDatabase("testDatabase");
  }
}


TestQueries();

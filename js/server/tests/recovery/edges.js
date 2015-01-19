
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  var c, v, e, i;
  db._drop("UnitTestsVertices");
  db._drop("UnitTestsEdges");

  v = db._create("UnitTestsVertices");
  e = db._createEdgeCollection("UnitTestsEdges");

  for (i = 0; i < 1000; ++i) {
    v.save({ _key: "node" + i, name: "some-name" + i });
  }

  for (i = 0; i < 1000; ++i) {
    e.save("UnitTestsVertices/node" + i, "UnitTestsVertices/node" + (i % 10), { _key: "edge" + i, what: "some-value" + i });
  }
   
  db._drop("test");
  c = db._create("test"); 
  c.save({ _key: "crashme" }, true); // wait for sync

  internal.debugSegfault("crashing server");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {
    },
    tearDown: function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test whether we can restore the trx data
////////////////////////////////////////////////////////////////////////////////
    
    testEdges : function () {
      var v, e, doc, i;
      
      v = db._collection("UnitTestsVertices");  
      assertEqual(1000, v.count());
      for (i = 0; i < 1000; ++i) {
        assertEqual("some-name" + i, v.document("node" + i).name);
      }
      
      e = db._collection("UnitTestsEdges");  
      for (i = 0; i < 1000; ++i) {
        doc = e.document("edge" + i);
        assertEqual("some-value" + i, doc.what);
        assertEqual("UnitTestsVertices/node" + i, doc._from);
        assertEqual("UnitTestsVertices/node" + (i % 10), doc._to);
      }

      for (i = 0; i < 1000; ++i) {
        assertEqual(1, e.outEdges("UnitTestsVertices/node" + i).length);
        if (i >= 10) {
          assertEqual(0, e.inEdges("UnitTestsVertices/node" + i).length);
        }
        else {
          assertEqual(100, e.inEdges("UnitTestsVertices/node" + i).length);
        }
      }
    }
        
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

function main (argv) {
  if (argv[1] === "setup") {
    runSetup();
    return 0;
  }
  else {
    jsunity.run(recoverySuite);
    return jsunity.done().status ? 0 : 1;
  }
}


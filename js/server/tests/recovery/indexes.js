
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;

  c.ensureHashIndex("value1");
  c.ensureSkiplist("value2");
  c.ensureCapConstraint(5000);

  for (i = 0; i < 1000; ++i) {
    c.save({ _key: "test" + i, value1: i, value2: "test" + (i % 10) });
  }

  c.save({ _key: "crashme" }, true);

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
    
    testIndexes : function () {
      var c = db._collection("UnitTestsRecovery"), i;
      var idx = c.getIndexes().sort(function (l, r) {
        if (l.id.length != r.id.length) {
          return l.id.length - r.id.length < 0 ? -1 : 1;
        }

        // length is equal
        for (var i = 0; i < l.id.length; ++i) {
          if (l.id[i] != r.id[i]) {
            return l.id[i] < r.id[i] ? -1 : 1;
          }
        }

        return 0;
      });

      assertEqual(4, idx.length);

      assertEqual("primary", idx[0].type);
      assertEqual("hash", idx[1].type);
      assertEqual([ "value1" ], idx[1].fields);
      var hash = idx[1];
      
      assertEqual("skiplist", idx[2].type);
      assertEqual([ "value2" ], idx[2].fields);
      var skip = idx[2];
      
      assertEqual("cap", idx[3].type);
      assertEqual(5000, idx[3].size);
      assertEqual(0, idx[3].byteSize);

      for (i = 0; i < 1000; ++i) {
        var doc = c.document("test" + i);
        assertEqual(i, doc.value1); 
        assertEqual("test" + (i % 10), doc.value2); 
      }

      for (i = 0; i < 1000; ++i) {
        var docs = c.byExampleHash(hash.id, { value1: i }).toArray();
        assertEqual(1, docs.length);
        assertEqual("test" + i, docs[0]._key);
      }

      for (i = 0; i < 1000; ++i) {
        var docs = c.byExampleSkiplist(skip.id, { value2: "test" + (i % 10) }).toArray();
        assertEqual(100, docs.length);
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


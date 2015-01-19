
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  var c;
  db._drop("footest");
  c = db._create("footest", { id: 99999999 });
  c.save({ foo: "bar" });
  c.save({ foo: "bart" });
  db._drop("footest");

  internal.wait(2);

  c = db._create("footest", { id: 99999999 });
  c.save({ foo: "baz" });

  db._drop("test");
  c = db._create("test");
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
    
    testCreateDatabase : function () {
      var c = db._collection("footest");
      assertEqual(1, c.count());
      assertEqual("baz", c.toArray()[0].foo);
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


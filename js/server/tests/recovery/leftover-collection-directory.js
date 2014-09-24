
var db = require("org/arangodb").db;
var internal = require("internal");
var fs = require("fs");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();

  var i, c, ids = [ ];
  for (i = 0; i < 10; ++i) {
    db._drop("UnitTestsRecovery" + i);
    c = db._create("UnitTestsRecovery" + i);
    ids[i] = c._id;
    c.drop();
  }
  db._drop("test");
  c = db._create("test");
  c.save({ _key: "crashme" }, true);

  internal.wait(3, true);
  for (i = 0; i < 10; ++i) {
    var path = fs.join(db._path(), "collection-" + ids[i]);
    fs.makeDirectory(path);

    if (i < 5) {
      // create a leftover parameter.json.tmp file
      fs.write(fs.join(path, "parameter.json.tmp"), "");
    }
    else {
      // create an empty parameter.json file
      fs.write(fs.join(path, "parameter.json"), "");
      // create some other file
      fs.write(fs.join(path, "foobar"), "foobar");
    }
  }

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
/// @brief test whether we can recover the collections even with the
/// leftover directories present
////////////////////////////////////////////////////////////////////////////////
    
    testLeftoverCollectionDirectory : function () {
      var i; 
      for (i = 0; i < 5; ++i) {
        assertEqual(null, db._collection("UnitTestsRecovery" + i));
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
    return jsunity.done() ? 0 : 1;
  }
}


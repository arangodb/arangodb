
var db = require("org/arangodb").db;
var internal = require("internal");
var fs = require("fs");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();

  var i, paths = [ ];
  for (i = 0; i < 3; ++i) {
    db._useDatabase("_system");
    try {
      db._dropDatabase("UnitTestsRecovery" + i);
    }
    catch (err) {
    }

    db._createDatabase("UnitTestsRecovery" + i);
    db._useDatabase("UnitTestsRecovery" + i);
    paths[i] = db._path();
    
    db._useDatabase("_system");
    db._dropDatabase("UnitTestsRecovery" + i);
  }

  db._drop("test");
  c = db._create("test");
  c.save({ _key: "crashme" }, true);

  internal.wait(3, true);
  for (i = 0; i < 3; ++i) {
    // re-create database directory
    try {
      fs.makeDirectory(paths[i]);
    }
    catch (err2) {
    }

    fs.write(fs.join(paths[i], "parameter.json"), "");
    fs.write(fs.join(paths[i], "parameter.json.tmp"), "");

    // create some collection directory
    fs.makeDirectory(fs.join(paths[i], "collection-123"));
    if (i < 2) {
      // create a leftover parameter.json.tmp file
      fs.write(fs.join(paths[i], "collection-123", "parameter.json.tmp"), "");
    }
    else {
      // create an empty parameter.json file
      fs.write(fs.join(paths[i], "collection-123", "parameter.json"), "");
      // create some other file
      fs.write(fs.join(paths[i], "collection-123", "foobar"), "foobar");
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
/// @brief test whether we can recover the databases even with the
/// leftover directories present
////////////////////////////////////////////////////////////////////////////////
    
    testLeftoverDatabaseDirectory : function () {
      assertEqual([ "_system" ], db._listDatabases());
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


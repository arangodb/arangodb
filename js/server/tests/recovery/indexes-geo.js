
var db = require("org/arangodb").db;
var internal = require("internal");
var simple = require("org/arangodb/simple-query");
var jsunity = require("jsunity");

function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery1");
  var c = db._create("UnitTestsRecovery1"), i, j;
  c.ensureGeoIndex("loc");
  c.ensureGeoIndex("lat", "lon");

  for (i = -40; i < 40; ++i) {
    for (j = -40; j < 40; ++j) {
      c.save({ loc: [ i, j ], lat: i, lon: j });
    }
  }

  db._drop("UnitTestsRecovery2");
  c = db._create("UnitTestsRecovery2");
  c.ensureGeoConstraint("a.loc", true, true);

  for (i = -40; i < 40; ++i) {
    for (j = -40; j < 40; ++j) {
      c.save({ a: { loc: [ i, j ] } });
    }
  }
  
  db._drop("UnitTestsRecovery3");
  c = db._create("UnitTestsRecovery3");
  c.ensureGeoConstraint("a.lat", "a.lon", false);

  for (i = -40; i < 40; ++i) {
    for (j = -40; j < 40; ++j) {
      c.save({ a: { lat: i, lon: j } });
    }
  }

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
    
    testIndexesGeo : function () {
      var c = db._collection("UnitTestsRecovery1"), idx, i;
      var geo1 = null, geo2 = null; 
      idx = c.getIndexes();
 
      for (i = 1; i < idx.length; ++i) {
        if (idx[i].type === 'geo1') {
          geo1 = idx[i];
          assertFalse(geo1.unique);
          assertFalse(geo1.ignoreNull);
          assertFalse(geo1.geoJson);
          assertEqual([ "loc" ], geo1.fields);
        }
        else if (idx[i].type === 'geo2') {
          geo2 = idx[i];
          assertFalse(geo2.unique);
          assertFalse(geo2.ignoreNull);
          assertEqual([ "lat", "lon" ], geo2.fields);
        }
      }

      assertNotNull(geo1);
      assertNotNull(geo2);

      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geo1.id).limit(100).toArray().length);
      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geo2.id).limit(100).toArray().length);


      c = db._collection("UnitTestsRecovery2");
      geo1 = c.getIndexes()[1];
      assertTrue(geo1.unique);
      assertTrue(geo1.ignoreNull);
      assertTrue(geo1.geoJson);
      assertEqual([ "a.loc" ], geo1.fields);

      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geo1.id).limit(100).toArray().length);


      c = db._collection("UnitTestsRecovery3");
      geo1 = c.getIndexes()[1];
      assertTrue(geo1.unique);
      assertFalse(geo1.ignoreNull);
      assertFalse(geo1.geoJson);
      assertEqual([ "a.lat", "a.lon" ], geo1.fields);

      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geo1.id).limit(100).toArray().length);
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


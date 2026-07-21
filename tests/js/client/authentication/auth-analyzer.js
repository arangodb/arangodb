/*jshint globalstrict:false, strict:false */
/* global getOptions, fail */


const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse} = jsunity.jsUnity.assertions;
const arango = require("@arangodb").arango;
const db = require("@arangodb").db;
const users = require('@arangodb/users');
const helper = require('@arangodb/testutils/user-helper');
let IM = global.instanceManager;

function testSuite() {
  const user = 'bob';

  const system = "_system";

  const rocol  = "readOnlyCollection";
  const rwcol  = "readWriteCollection";

  const rodb  = "readOnlyDatabase";
  const rwdb  = "readWriteDatabase";

  const name = "TestAuthAnalyzer";

  if (!users.exists('bob')) {
    users.save(user, ''); // password must be empty otherwise switchUser will not work
  }

  // analyzers can only be changed from the `_system` database
  // analyzer API does not support database selection via the usual `_db/<dbname>/_api/<api>`

  return {
    setUp: function() {
      IM.rememberConnection();

      db._createDatabase(rodb);
      db._createDatabase(rwdb);

      db._create(rwcol);
      db._useDatabase(rodb);
      db._create(rocol);
      db._useDatabase(rwdb);
      db._create(rwcol);

      db._useDatabase(system);

      users.grantDatabase(user, system, "ro");
      users.grantDatabase(user, rodb, "ro");
      users.grantDatabase(user, rwdb, "rw");
      users.grantCollection(user, system, "*", "none");
      users.grantCollection(user, rwdb,   "*", "rw");
      users.grantCollection(user, rodb,   "*", "ro");
 
      users.reload();
    },

    tearDown: function() {
      helper.switchUser("root", system);

      db._drop(rwcol);
      db._dropDatabase(rwdb);
      db._dropDatabase(rodb);

      arango.DELETE("/_api/analyzer/" + name + "?force=true");

      IM.reconnectMe();
    },

    testAnalyzerGet : function() {
      helper.switchUser(user, system);
      let result = arango.GET("/_api/analyzer");
      assertEqual(result.error, false);
      assertEqual(result.code, 200);
    },

    testAnalyzerCreateTextInRwCol : function() {
      helper.switchUser(user, rwdb);
      let body = JSON.stringify({
        type : "text",
        name : name,
        properties : { locale: "en.UTF-8", stopwords: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    testAnalyzerCreateTextInRoCol : function() {
      helper.switchUser(user, rodb);
      let body = JSON.stringify({
        type : "text",
        name : name,
        properties : { locale: "en.UTF-8", stopwords: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertTrue(result.error);
      assertEqual(result.code, 403);
    },

  }; // return

} // end of suite
jsunity.run(testSuite);
return jsunity.done();

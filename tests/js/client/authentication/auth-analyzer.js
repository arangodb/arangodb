/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */


const jsunity = require('jsunity');
const internal = require('internal');
const error = internal.errors;
const print = internal.print;
const arango = internal.arango;

const db = require("@arangodb").db;
const users = require('@arangodb/users');
const helper = require('@arangodb/user-helper');
const endpoint = arango.getEndpoint();


function testSuite() {
  const jwtSecret = 'haxxmann';
  const user = 'hackers@arangodb.com';

  const system = "_system";

  const rocol  = "readOnlyCollection";
  const rwcol  = "readWriteCollection";

  const rodb  = "readOnlyDatabase";
  const rwdb  = "readWriteDatabase";

  const name = "TestAuthAnalyzerView";

  users.save(user, ''); // password must be empty otherwise switchUser will not work

  return {
    setUp: function() {

      db._createDatabase(rodb);
      db._createDatabase(rwdb);

      db._create(rwcol);
      db._useDatabase(rodb);
      db._create(rocol);
      db._useDatabase(rwdb);
      db._create(rwcol);

      db._useDatabase(system);


      users.grantDatabase(user, db._name());
      users.grantDatabase(user, rocol);
      users.grantDatabase(user, rwcol);
      users.grantCollection(user, db._name(), "*", "ro");
      users.grantCollection(user, rwdb, "*", "rw");
      users.grantCollection(user, rodb, "*", "ro");
      users.reload();

      helper.switchUser(user, system, "");
    },

    tearDown: function() {
      helper.switchUser("root", system);
      db._drop(rwcol);
      db._dropDatabase(rwdb);
      db._dropDatabase(rodb);

      arango.DELETE("/_api/analyzer/" + name + "?force=true");
    },

    testAnalyzerGet : function() {

      let headers = {
        "Authorization" : "Bearer " + jwtSecret,
      };

      let result = arango.GET("/_api/analyzer", headers);
      assertEqual(result.error, false);
      assertEqual(result.code, 200);
    },

    testAnalyzerCreateTextInRwCol : function() {
      let body = JSON.stringify({
        type : "text",
        name : rwdb + "::" + name,
        properties : { locale: "en.UTF-8", ignored_words: [ ] },
      });

      let result = arango.POST_RAW("/_api/analyzer", body);
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

    testAnalyzerCreateTextInRoCol : function() {
      let body = JSON.stringify({
        type : "text",
        name : rodb + "::" + name,
        properties : { locale: "en.UTF-8", ignored_words: [ ] },
      });

      let headers = {
        "Authorization" : "Bearer " + jwtSecret,
      };

      let result = arango.POST_RAW("/_api/analyzer", body, headers);
      assertFalse(result.error);
      assertEqual(result.code, 201);
    },

  }; // return

} // end of suite
jsunity.run(testSuite);
return jsunity.done();

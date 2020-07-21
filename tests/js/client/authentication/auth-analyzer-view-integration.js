/*jshint globalstrict:false, strict:false */
/* global assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */

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
  const user = 'bob';
  const system = "_system";

  const name = "TestAuthAnalyzer";

  return {
    setUp: function() {
      helper.switchUser("root", system);
      try {
        users.remove(user);
      } catch (err) {}
      
      try {
        db._dropDatabase(name);
      } catch (err) {}
    },

    tearDown: function() {
      helper.switchUser("root", system);
      try {
        db._dropDatabase(name);
      } catch (err) {}

      try {
        users.remove(user);
      } catch (err) {}
    },

    testIntegration : function() {
      db._createDatabase(name);
    
      users.save(user, "", true); 
      users.grantDatabase(user, name, "rw"); 

      helper.switchUser(user, name);
    
      db._createView("test", "arangosearch", {});
    
      let view = db._view("test");
      view.properties({links:{}});
    },

  };
} 

jsunity.run(testSuite);
return jsunity.done();

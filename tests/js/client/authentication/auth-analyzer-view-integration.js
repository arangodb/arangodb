/*jshint globalstrict:false, strict:false */
/* global assertTrue, assertFalse, assertEqual, fail */

const jsunity = require('jsunity');
const analyzers = require("@arangodb/analyzers");
const db = require("@arangodb").db;
const users = require('@arangodb/users');
const helper = require('@arangodb/testutils/user-helper');
const internal = require('internal');

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
      const analyzerName = "more_text_de";

      analyzers.save(analyzerName, "text",
                   { locale: "de.UTF-8", stopwords: [ ] },
                   [ "frequency", "norm", "position" ]);

      try {
        db._createDatabase(name);
        // create new user for new database.
        // user does not have any permissions on _system database, thus
        // no permissions on the above analyzer
        users.save(user, "", true); 
        users.grantDatabase(user, name, "rw"); 

        helper.switchUser(user, name);

        db._createView("test1", "arangosearch", {});
        let view = db._view("test1");
        view.properties({links:{}});
        
        db._createView("test2", "search-alias", {});
        view = db._view("test2");
        view.properties({indexes:[]});
       
        try {
          analyzers.remove(analyzerName);
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
        }
      } finally {
        helper.switchUser("root", system);
        analyzers.remove(analyzerName);
      }
    },

  };
} 

jsunity.run(testSuite);
return jsunity.done();

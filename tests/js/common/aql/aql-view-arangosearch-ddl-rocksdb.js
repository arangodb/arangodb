/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */


const jsunity = require('jsunity');
const internal = require('internal');
const error = internal.errors;
//const print = internal.print;

function testSuite() {
  const endpoint = arango.getEndpoint();
  const db = require("@arangodb").db;
  const dbName = 'TestDB';
  return {
    setUp: function() {
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
    },
    tearDown: function() {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },
    ////////////////////////////////////////////////////////////////////////////
    /// @brief test creating link with 'inBackground' set to true
    ////////////////////////////////////////////////////////////////////////////
    testCreateLinkInBackgroundMode: function () {
      const colName = 'TestCollection';
      const viewName = 'TestView';
      const initialCount = 1000;
      const inTransCount = 1000;

      db._useDatabase(dbName);
      let col = db._create(colName);
      let v = db._createView(viewName, 'arangosearch', {});
      // some initial documents
      for (let i = 0; i < initialCount; ++i) {
        col.insert({ myField: 'test' + i }); 
      } 
      // transaction in separate thread, which will populate collection
      let transBody = JSON.stringify({
        collections: { write: colName },
        action: "function (params) { " +
          "var db = require('internal').db; " +
          "db._useDatabase(params.dbName);" +
          "var c = db._collection(params.colName); " +
          "for (var i = 0; i < params.inTransCount; ++i) { c.insert({ myField: 'background' + i }); } " +
          "require('internal').sleep(20)" + 
          "}",
        params: { colName, inTransCount, dbName }
      });
      let transResult = arango.POST_RAW("/_api/transaction", transBody, { "x-arango-async" : "true" });
      assertEqual(202, transResult.code);
      require('internal').sleep(5); // give transaction some time to run before us
      v.properties({ links: { [colName]: { includeAllFields: true, inBackground: true } } });
      // check that all documents are visible
      let docs = db._query("FOR doc IN " + viewName  + " OPTIONS { waitForSync: true } RETURN doc").toArray();
      assertEqual(initialCount + inTransCount, docs.length);

      // inBackground should not be returned as part of index definition
      let indexes = col.getIndexes(false, true);
      assertEqual(2, indexes.length);
      var link = indexes[1];
      assertEqual("arangosearch", link.type);
      assertTrue(undefined === link.inBackground);

       // inBackground should not be returned as part of link definition
      let propertiesReturned = v.properties();
      assertTrue(undefined === propertiesReturned.links[colName].inBackground);
    },
  }; // return
} // end of suite

jsunity.run(testSuite);
return jsunity.done();

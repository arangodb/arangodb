/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual */

const jsunity = require('jsunity');
const tasks = require('@arangodb/tasks');
const fs = require('fs');

function testSuite() {
  const db = require("@arangodb").db;
  const dbName = 'TestDB';
  const taskCreateLinkInBackground = 'CreateLinkInBackgroundMode_AuxTask';
  return {
    setUp: function() {
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
    },
    tearDown: function() {
      try { tasks.unregister(taskCreateLinkInBackground); } catch(e) {}
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },
    ////////////////////////////////////////////////////////////////////////////
    /// @brief test creating link with 'inBackground' set to true
    ////////////////////////////////////////////////////////////////////////////
    testCreateLinkInBackgroundMode: function () {
      const colName = 'TestCollection';
      const viewName = 'TestView';
      const initialCount = 500;
      const inTransCount = 1000;
      const markerFileName = fs.join(fs.getTempPath(), "backgroundLinkMarker");
      try { fs.remove(markerFileName); } catch(e) {}
      db._useDatabase(dbName);
      let col = db._create(colName);
      let v = db._createView(viewName, 'arangosearch', {});
      // some initial documents
      for (let i = 0; i < initialCount; ++i) {
        col.insert({ myField: 'test' + i }); 
      } 
      let commandText = function (params) { 
          var db = require('internal').db; 
          db._executeTransaction({
              collections:  { write: params.colName },
               action: function(params) {
                var fs = require('fs');
                var db = require('internal').db; 
                var c = db._collection(params.colName); 
                fs.write(params.markerFileName, "TEST");
                for (var i = 0; i < params.inTransCount; ++i) { 
                  c.insert({ myField: 'background' + i }); 
                } 
                require('internal').sleep(20); 
              },
              params: params
            });
          };
      tasks.register({
        command: commandText,
        params: { colName, inTransCount, dbName, markerFileName },
        name: taskCreateLinkInBackground
      });
      while(!fs.exists(markerFileName)) {
         require('internal').sleep(1); // give transaction some time to run 
      }
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

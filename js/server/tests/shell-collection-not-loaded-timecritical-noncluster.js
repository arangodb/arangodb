/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the random document selector 
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("org/arangodb");
var db = arangodb.db;
var internal = require("internal");
var ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;

// -----------------------------------------------------------------------------
// --SECTION--                                       throw-collection-not-loaded
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ThrowCollectionNotLoadedSuite () {
  'use strict';
  var old;
  var cn = "UnitTestsThrowCollectionNotLoaded";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      // fetch current settings
      old = internal.throwOnCollectionNotLoaded();
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
      // restore old settings
      internal.throwOnCollectionNotLoaded(old);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test regular loading of collection
////////////////////////////////////////////////////////////////////////////////

    testLoad : function () {
      internal.throwOnCollectionNotLoaded(false);

      var c = db._create(cn);
      c.save({ value: 1 });

      c.unload();
      c = null;
      internal.wal.flush(true, true);
      while (db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.wait(0.5);
      }

      db._collection(cn).load();
      assertEqual(ArangoCollection.STATUS_LOADED, db._collection(cn).status());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test regular loading of collection, but with flag turned on
////////////////////////////////////////////////////////////////////////////////

    testLoadWithFlag : function () {
      internal.throwOnCollectionNotLoaded(true);

      var c = db._create(cn);
      c.save({ value: 1 });

      c.unload();
      c = null;
      internal.wal.flush(true, true);
      while (db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.wait(0.5);
      }

      db._collection(cn).load();
      assertEqual(ArangoCollection.STATUS_LOADED, db._collection(cn).status());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parallel loading of collection
////////////////////////////////////////////////////////////////////////////////

    testLoadParallel : function () {
      internal.throwOnCollectionNotLoaded(false);
      var tasks = require("org/arangodb/tasks");

      var c = db._create(cn);
      for (var i = 0; i < 10000; ++i) {
        c.save({ value: 1 });
      }

      db._drop(cn + "Collect");
      var cnCollect = cn + "Collect";
      db._create(cnCollect);

      c.unload();
      c = null;
      internal.wal.flush(true, true);
      while (db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.wait(0.5);
      }
    
      var task = {
        offset: 0,
        params: { cn: cn },
        command: function (params) {
          var db = require('internal').db;
          try {
            for (var i = 0; i < 500; ++i) {
              db._collection(params.cn).load();
              db._collection(params.cn).unload();
            }
          }
          catch (err) {
            db._collection(params.cn + "Collect").save({ err: err.errorNum });
          }
        }
      };

      // spawn a few tasks that load and unload
      for (i = 0; i < 20; ++i) {
        task.id = "loadtest" + i;
        tasks.register(task);
      }

      // wait for tasks to join
      internal.wait(5);
       
      var errors = internal.errors;

      var found = db._collection(cnCollect).byExample({ 
        err: errors.ERROR_ARANGO_COLLECTION_NOT_LOADED.code 
      }).toArray();
      db._drop(cnCollect);

      // we should have seen no "collection not found" errors
      assertEqual(0, found.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parallel loading of collection, with flag
////////////////////////////////////////////////////////////////////////////////

    testLoadParallelWithFlag : function () {
      internal.throwOnCollectionNotLoaded(true);
      var tasks = require("org/arangodb/tasks");

      var c = db._create(cn);
      for (var i = 0; i < 10000; ++i) {
        c.save({ value: 1 });
      }

      db._drop(cn + "Collect");
      var cnCollect = cn + "Collect";
      db._create(cnCollect);

      c.unload();
      c = null;
      internal.wal.flush(true, true);
      while (db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.wait(0.5);
      }
    
      var task = {
        offset: 0,
        params: { cn: cn },
        command: function (params) {
          var db = require('internal').db;
          try {
            for (var i = 0; i < 500; ++i) {
              db._collection(params.cn).load();
              db._collection(params.cn).unload();
            }
          }
          catch (err) {
            db._collection(params.cn + "Collect").save({ err: err.errorNum });
          }
        }
      };

      // spawn a few tasks that load and unload
      for (i = 0; i < 20; ++i) {
        task.id = "loadtest" + i;
        tasks.register(task);
      }

      // wait for tasks to join
      internal.wait(5);
       
      var errors = internal.errors;

      var found = db._collection(cnCollect).byExample({ 
        err: errors.ERROR_ARANGO_COLLECTION_NOT_LOADED.code 
      }).toArray();
      db._drop(cnCollect);

      // we need to have seen at least one "collection not found" error
      assertTrue(found.length > 0);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ThrowCollectionNotLoadedSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:


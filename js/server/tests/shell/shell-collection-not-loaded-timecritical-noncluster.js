/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var internal = require("internal");

var db = arangodb.db;
var ArangoCollection = require("@arangodb/arango-collection").ArangoCollection;

function ThrowCollectionNotLoadedSuite() {
  'use strict';
  var old;
  var cn = "UnitTestsThrowCollectionNotLoaded";

  return {
    setUp: function() {
      // fetch current settings
      old = internal.throwOnCollectionNotLoaded();
      db._drop(cn);
    },

    tearDown: function() {
      db._drop(cn);
      // restore old settings
      internal.throwOnCollectionNotLoaded(old);
    },

    // test regular loading of collection
    testLoad: function() {
      internal.throwOnCollectionNotLoaded(false);

      var c = db._create(cn);
      c.save({
        value: 1
      });

      c.unload();
      c = null;
      internal.wal.flush(true, true);
      while (db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.wait(0.5);
      }

      db._collection(cn).load();
      assertEqual(ArangoCollection.STATUS_LOADED, db._collection(cn).status());
    },

    // test regular loading of collection, but with flag turned on
    testLoadWithFlag: function() {
      internal.throwOnCollectionNotLoaded(true);

      var c = db._create(cn);
      c.save({
        value: 1
      });

      c.unload();
      c = null;
      internal.wal.flush(true, true);
      while (db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.wait(0.5);
      }

      db._collection(cn).load();
      assertEqual(ArangoCollection.STATUS_LOADED, db._collection(cn).status());
    },

    // test parallel loading of collection
    testLoadParallel: function() {
      internal.throwOnCollectionNotLoaded(false);
      var tasks = require("@arangodb/tasks");
      var c = db._create(cn);

      for (var i = 0; i < 10000; ++i) {
        c.save({
          value: 1
        });
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
        params: {
          cn: cn
        },
        command: function(params) {
          var db = require('internal').db;
          var result = db._collection(params.cn + "Collect");

          try {
            for (var i = 0; i < 100; ++i) {
              db._collection(params.cn).load();
              db._collection(params.cn).unload();
            }
          } catch (err) {
            result.save({
              err: err.errorNum
            });
            return;
          }

          result.save({
            done: true
          });
        }
      };

      // spawn a few tasks that load and unload
      let iter = 20;

      for (i = 0; i < iter; ++i) {
        task.id = "loadtest" + i;
        tasks.register(task);
      }

      // wait for tasks to join
      let rc = db._collection(cnCollect);

      while (rc.count() < iter) {
        internal.wait(0.5);
      }

      // check for errors
      var errors = internal.errors;

      var found = rc.byExample({
        err: errors.ERROR_ARANGO_COLLECTION_NOT_LOADED.code
      }).toArray();
      db._drop(cnCollect);

      // we should have seen no "collection not found" errors
      assertEqual(0, found.length);
    },

    // test parallel loading of collection, with flag
    testLoadParallelWithFlag: function() {
      internal.throwOnCollectionNotLoaded(true);
      var tasks = require("@arangodb/tasks");
      var c = db._create(cn);

      for (var i = 0; i < 10000; ++i) {
        c.save({
          value: 1
        });
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
        params: {
          cn: cn
        },
        command: function(params) {
          var db = require('internal').db;
          var result = db._collection(params.cn + "Collect");

          try {
            for (var i = 0; i < 500; ++i) {
              db._collection(params.cn).load();
              db._collection(params.cn).unload();
            }
          } catch (err) {
            db._collection(params.cn + "Collect").save({
              err: err.errorNum
            });
            return;
          }

          result.save({
            done: true
          });
        }
      };

      // spawn a few tasks that load and unload
      let iter = 20;

      for (i = 0; i < iter; ++i) {
        task.id = "loadtest" + i;
        tasks.register(task);
      }

      // wait for tasks to join
      let rc = db._collection(cnCollect);

      while (rc.count() < iter) {
        internal.wait(0.5);
      }

      // check for errors
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

jsunity.run(ThrowCollectionNotLoadedSuite);
return jsunity.done();

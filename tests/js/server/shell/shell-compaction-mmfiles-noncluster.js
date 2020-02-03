/* jshint globalstrict:false, strict:false, sub: true */
/* global fail, assertEqual, assertTrue, assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the compaction
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var internal = require('internal');
var testHelper = require('@arangodb/test-helper').Helper;
var ArangoCollection = require('@arangodb/arango-collection').ArangoCollection;
const console = require('console');
const cn = 'UnitTestsCompaction';

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: collection
// //////////////////////////////////////////////////////////////////////////////

// This function waits for the compactionState of collection to change
// and a compaction to have occurred.
//
// We define a compaction to have occured when files have been combined.
// Note that the compaction count only counts how many times the compaction
// thread has run, but the thread might not have performed any action
//
// The function tries nrTries iterations in which it waits for wait seconds
//
// returns true if a compaction occurred, and false if there was no
// compaction detected.
function waitForCompaction(collection, initialFigures, nrTries, wait) {
  var lastTime = initialFigures["compactionStatus"]["time"];
  var lastMessage = initialFigures["compactionStatus"]["message"];
  var lastCount = initialFigures["compactionStatus"]["count"];

  console.log("waiting for compaction to occur");
  var tries = 0;
  while (tries < nrTries) {
    var cur = collection.figures();
    if (cur["compactionStatus"]["time"] !== lastTime) {
      lastTime = cur["compactionStatus"]["time"];
      lastMessage = cur["compactionStatus"]["message"];
      lastCount = cur["compactionStatus"]["count"];
      console.log("compaction timestamp changed: " + lastTime + ", message: " + lastMessage);
      if (cur.compactionStatus.filesCombined > 0) {
        console.log("compaction occurred");
        return true;
      }
    }
    internal.wait(wait, false);
    tries++;
  }
  return false;
}

function CompactionSuite () {
  'use strict';
  return {
    setUp : function () {
      internal.db._drop(cn);
    },

    tearDown : function () {
      internal.db._drop(cn);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief create movement of shapes
    // //////////////////////////////////////////////////////////////////////////////

    testShapesMovement: function () {
      const ndocs = 10000;
      const nrRubbish = 250000;

      let collection = internal.db._create(cn, {
        'journalSize': 1048576
      });
      var x, i, j, doc;


      const initialFigures = collection.figures();

      for (i = 0; i < ndocs; ++i) {
        doc = {
          _key: 'old' + i,
          a: i,
          b: 'test' + i,
          values: [ ],
          atts: { } };
        x = { };
        for (j = 0; j < 10; ++j) {
          doc.values.push(j);
          doc.atts['test' + i + j] = 'test';
          x['foo' + i] = [ '1' ];
        }
        doc.atts.foo = x;
        collection.save(doc);
      }

      // now access the documents once, to build the shape accessors
      for (i = 0; i < ndocs; ++i) {
        doc = collection.document('old' + i);

        assertTrue(doc.hasOwnProperty('a'));
        assertEqual(i, doc.a);
        assertTrue(doc.hasOwnProperty('b'));
        assertEqual('test' + i, doc.b);
        assertTrue(doc.hasOwnProperty('values'));
        assertEqual(10, doc.values.length);
        assertTrue(doc.hasOwnProperty('atts'));
        assertEqual(11, Object.keys(doc.atts).length);

        for (j = 0; j < 10; ++j) {
          assertEqual('test', doc.atts['test' + i + j]);
        }
        for (j = 0; j < 10; ++j) {
          assertEqual([ '1' ], doc.atts.foo['foo' + i]);
        }
      }

      // fill the datafile with rubbish
      for (i = 0; i < nrRubbish; ++i) {
        collection.save({
          _key: 'test' + i,
          value: 'thequickbrownfox'
        });
      }

      for (i = 0; i < nrRubbish; ++i) {
        collection.remove('test' + i);
      }

      /* we wait for a compaction to occur */
      if (waitForCompaction(collection, initialFigures, 100, 1) === false) {
          throw "No compaction occurred!";
      }
      assertEqual(ndocs, collection.count());

      // now access the 'old' documents, which were probably moved
      for (i = 0; i < ndocs; ++i) {
        doc = collection.document('old' + i);

        assertTrue(doc.hasOwnProperty('a'));
        assertEqual(i, doc.a);
        assertTrue(doc.hasOwnProperty('b'));
        assertEqual('test' + i, doc.b);
        assertTrue(doc.hasOwnProperty('values'));
        assertEqual(10, doc.values.length);
        assertTrue(doc.hasOwnProperty('atts'));
        assertEqual(11, Object.keys(doc.atts).length);

        for (j = 0; j < 10; ++j) {
          assertEqual('test', doc.atts['test' + i + j]);
        }
        for (j = 0; j < 10; ++j) {
          assertEqual([ '1' ], doc.atts.foo['foo' + i]);
        }
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test shapes
    // //////////////////////////////////////////////////////////////////////////////

    testShapes1: function () {
      const nrRubbish = 10000;
      const nrShapes = 10000;
      var c1 = internal.db._create(cn, {
        'journalSize': 1048576
      });
      const initialFigures = c1.figures();

      var i, doc;

      // prefill with 'trash'
      for (i = 0; i < nrRubbish; ++i) {
        c1.save({
          _key: 'test' + i
        });
      }

      // this accesses all documents, and creates shape accessors for all of them
      c1.toArray();

      c1.truncate();
      testHelper.rotate(c1);

      // create lots of different shapes
      for (i = 0; i < nrShapes; ++i) {
        doc = {
          _key: 'test' + i
        };
        doc['number' + i] = i;
        doc['string' + i] = 'test' + i;
        doc['bool' + i] = (i % 2 === 0);
        c1.save(doc);
      }

      // make sure compaction moves the shapes
      testHelper.rotate(c1);
      c1.truncate();

      if (waitForCompaction(c1, initialFigures, 100, 1) === false) {
        throw "No compaction occurred!";
      }

      for (i = 0; i < 100; ++i) {
        doc = {
          _key: 'test' + i
        };
        doc['number' + i] = i + 1;
        doc['string' + i] = 'test' + (i + 1);
        doc['bool' + i] = (i % 2 !== 0);
        c1.save(doc);
      }

      for (i = 0; i < 100; ++i) {
        doc = c1.document('test' + i);
        assertTrue(doc.hasOwnProperty('number' + i));
        assertTrue(doc.hasOwnProperty('string' + i));
        assertTrue(doc.hasOwnProperty('bool' + i));

        assertEqual(i + 1, doc['number' + i]);
        assertEqual('test' + (i + 1), doc['string' + i]);
        assertEqual(i % 2 !== 0, doc['bool' + i]);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test shapes
    // //////////////////////////////////////////////////////////////////////////////

    testShapes2: function () {
      var c1 = internal.db._create(cn, {
        'journalSize': 1048576
      });
      const initialFigures = c1.figures();

      var i, doc;
      // prefill with 'trash'
      for (i = 0; i < 1000; ++i) {
        c1.save({
          _key: 'test' + i
        });
      }
      c1.truncate();
      testHelper.rotate(c1);

      // create lots of different shapes
      for (i = 0; i < 100; ++i) {
        doc = {
          _key: 'test' + i
        };
        doc['number' + i] = i;
        doc['string' + i] = 'test' + i;
        doc['bool' + i] = (i % 2 === 0);
        c1.save(doc);
      }

      c1.save({
        _key: 'foo',
        name: {
          first: 'foo',
          last: 'bar'
        }
      });
      c1.save({
        _key: 'bar',
        name: {
          first: 'bar',
          last: 'baz',
          middle: 'foo'
        },
        age: 22
      });

      // remove most of the shapes
      for (i = 0; i < 100; ++i) {
        c1.remove('test' + i);
      }

      // make sure compaction moves the shapes
      testHelper.rotate(c1);

      doc = c1.document('foo');
      assertTrue(doc.hasOwnProperty('name'));
      assertFalse(doc.hasOwnProperty('age'));
      assertEqual({
        first: 'foo',
        last: 'bar'
      }, doc.name);

      doc = c1.document('bar');
      assertTrue(doc.hasOwnProperty('name'));
      assertTrue(doc.hasOwnProperty('age'));
      assertEqual({
        first: 'bar',
        last: 'baz',
        middle: 'foo'
      }, doc.name);
      assertEqual(22, doc.age);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test shapes
    // //////////////////////////////////////////////////////////////////////////////

    testShapesUnloadReload: function () {
      var c1 = internal.db._create(cn, { 'journalSize': 1048576 });
      const initialFigures = c1.figures();

      var i, doc;

      // create lots of different shapes
      for (i = 0; i < 100; ++i) {
        doc = { _key: 'test' + i };
        doc['number' + i] = i;
        doc['string' + i] = 'test' + i;
        doc['bool' + i] = (i % 2 === 0);
        c1.save(doc);
      }

      c1.save({
        _key: 'foo',
        name: {
          first: 'foo',
          last: 'bar'
        }
      });
      c1.save({
        _key: 'bar',
        name: {
          first: 'bar',
          last: 'baz',
          middle: 'foo'
        },
        age: 22
      });

      // this accesses all documents, and creates shape accessors for all of them
      c1.toArray();

      // remove most of the shapes
      for (i = 0; i < 100; ++i) {
        c1.remove('test' + i);
      }

      // make sure compaction moves the shapes
      testHelper.rotate(c1);
      // unload the collection

      testHelper.waitUnload(c1);

      c1 = internal.db._collection(cn);

      // check if documents are still there
      doc = c1.document('foo');
      assertTrue(doc.hasOwnProperty('name'));
      assertFalse(doc.hasOwnProperty('age'));
      assertEqual({
        first: 'foo',
        last: 'bar'
      }, doc.name);

      doc = c1.document('bar');
      assertTrue(doc.hasOwnProperty('name'));
      assertTrue(doc.hasOwnProperty('age'));
      assertEqual({
        first: 'bar',
        last: 'baz',
        middle: 'foo'
      }, doc.name);

      assertEqual(22, doc.age);

      // create docs with already existing shapes
      for (i = 0; i < 100; ++i) {
        doc = { _key: 'test' + i };
        doc['number' + i] = i;
        doc['string' + i] = 'test' + i;
        doc['bool' + i] = (i % 2 === 0);
        c1.save(doc);
      }

      // check if the documents work
      for (i = 0; i < 100; ++i) {
        doc = c1.document('test' + i);
        assertTrue(doc.hasOwnProperty('number' + i));
        assertTrue(doc.hasOwnProperty('string' + i));
        assertTrue(doc.hasOwnProperty('bool' + i));

        assertEqual(i, doc['number' + i]);
        assertEqual('test' + i, doc['string' + i]);
        assertEqual(i % 2 === 0, doc['bool' + i]);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test journals
    // //////////////////////////////////////////////////////////////////////////////

    testJournals: function () {
      var c1 = internal.db._create(cn, {
        'journalSize': 1048576
      });
      const initialFigures = c1.figures();

      internal.wal.flush(true, true);

      // empty collection
      var fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig['alive']['count']);
      assertEqual(0, fig['dead']['count']);
      assertEqual(0, fig['dead']['size']);
      assertEqual(0, fig['dead']['deletion']);
      assertEqual(0, fig['journals']['count']);
      assertEqual(0, fig['datafiles']['count']);
      assertEqual(0, fig['compactors']['count']);

      c1.save({
        'foo': 'bar'
      });

      internal.wal.flush(true, true);

      var tries = 0;
      while (++tries < 20) {
        fig = c1.figures();
        if (fig['alive']['count'] === 1) {
          break;
        }
        internal.wait(1, false);
      }

      fig = c1.figures();
      assertEqual(c1.count(), 1);
      assertEqual(fig['alive']['count'], 1);
      assertTrue(fig['alive']['size'] > 0);
      assertEqual(fig['dead']['count'], 0);
      assertEqual(fig['dead']['size'], 0);
      assertEqual(fig['dead']['deletion'], 0);
      assertEqual(fig['journals']['count'], 1);
      assertEqual(fig['datafiles']['count'], 0);
      assertEqual(fig['compactors']['count'], 0);

      testHelper.rotate(c1);

      fig = c1.figures();
      assertEqual(c1.count(), 1);
      assertEqual(fig['alive']['count'], 1);
      assertTrue(fig['alive']['size'] > 0);
      assertEqual(fig['dead']['count'], 0);
      assertEqual(fig['dead']['size'], 0);
      assertEqual(fig['dead']['deletion'], 0);
      assertEqual(fig['journals']['count'], 0);
      assertEqual(fig['datafiles']['count'], 1);
      assertEqual(fig['compactors']['count'], 0);

      c1.save({
        'bar': 'baz'
      });

      internal.wal.flush(true, true);

      tries = 0;
      while (++tries < 20) {
        fig = c1.figures();

        if (fig['alive']['count'] === 2) {
          break;
        }
        internal.wait(1, false);
      }

      var alive = fig['alive']['size'];
      assertEqual(2, c1.count());
      assertEqual(2, fig['alive']['count']);
      assertTrue(alive > 0);
      assertEqual(0, fig['dead']['count']);
      assertEqual(0, fig['dead']['size']);
      assertEqual(0, fig['dead']['deletion']);
      assertEqual(1, fig['journals']['count']);
      assertEqual(1, fig['datafiles']['count']);
      assertEqual(0, fig['compactors']['count']);

      c1.properties({
        doCompact: false
      });

      internal.wait(1, false);

      testHelper.rotate(c1);

      fig = c1.figures();
      assertEqual(2, c1.count());
      assertEqual(2, fig['alive']['count']);
      assertEqual(alive, fig['alive']['size']);
      assertEqual(0, fig['dead']['count']);
      assertEqual(0, fig['dead']['size']);
      assertEqual(0, fig['dead']['deletion']);
      assertEqual(0, fig['journals']['count']);
      assertTrue(fig['datafiles']['count'] >= 1);
      assertEqual(0, fig['compactors']['count']);

      c1.properties({
        doCompact: true
      });
      internal.wait(1, false);

      c1.truncate();
      testHelper.rotate(c1);

      tries = 0;
      while (++tries < 20) {
        fig = c1.figures();
        if (fig['alive']['count'] === 0) {
          break;
        }
        require('internal').wait(0.1);
      }

      assertEqual(0, c1.count());
      assertEqual(0, fig['alive']['count']);
      assertEqual(0, fig['alive']['size']);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test figures after truncate and rotate
    // //////////////////////////////////////////////////////////////////////////////

    testFiguresTruncate: function () {
      var i;
      var maxWait;
      var n = 400;
      var payload = 'the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox. boom bang.';

      for (i = 0; i < 5; ++i) {
        payload += payload;
      }

      var c1 = internal.db._create(cn, {
        'journalSize': 1048576
      });

      for (i = 0; i < n; ++i) {
        c1.save({
          _key: 'test' + i,
          value: i,
          payload: payload
        });
      }

      testHelper.waitUnload(c1);

      var fig = c1.figures();
      c1.properties({
        doCompact: false
      });
      assertEqual(n, c1.count());
      assertEqual(n, fig['alive']['count']);
      assertEqual(0, fig['dead']['count']);
      assertEqual(0, fig['dead']['size']);
      assertEqual(0, fig['dead']['deletion']);
      assertEqual(1, fig['journals']['count']);
      assertTrue(fig['datafiles']['count'] > 0);

      c1.truncate();
      fig = c1.figures();

      assertEqual(0, c1.count());
      assertEqual(0, fig['alive']['count']);
      assertTrue(fig['dead']['count'] >= 0);
      assertTrue(fig['dead']['size'] >= 0);
      assertTrue(fig['dead']['deletion'] >= 0);
      assertEqual(1, fig['journals']['count']);
      assertTrue(fig['datafiles']['count'] >= 0);

      internal.wal.flush(true, true);
      c1.rotate();

      const initialFigures = c1.figures();
      c1.properties({
        doCompact: true
      });

      waitForCompaction(c1, initialFigures, 100, 1);

      fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig['alive']['count']);
      assertEqual(0, fig['alive']['size']);
      assertEqual(0, fig['dead']['count']);
      assertEqual(0, fig['dead']['size']);
      assertEqual(0, fig['dead']['deletion']);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test figures after truncate and rotate, with compaction disabled
    // //////////////////////////////////////////////////////////////////////////////

    testFiguresNoCompact: function () {
      var maxWait;
      var n = 400;
      var i;
      var payload = 'the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox. boom bang.';

      for (i = 0; i < 5; ++i) {
        payload += payload;
      }

      var c1 = internal.db._create(cn, {
        'journalSize': 1048576,
        'doCompact': false
      });

      for (i = 0; i < n; ++i) {
        c1.save({
          _key: 'test' + i,
          value: i,
          payload: payload
        });
      }

      testHelper.waitUnload(c1);

      var fig = c1.figures();
      assertEqual(n, c1.count());
      assertEqual(n, fig['alive']['count']);
      assertEqual(0, fig['dead']['count']);
      assertEqual(0, fig['dead']['size']);
      assertEqual(0, fig['dead']['deletion']);
      assertEqual(1, fig['journals']['count']);
      assertTrue(fig['datafiles']['count'] > 0);

      c1.truncate();
      internal.wal.flush(true, true);
      c1.rotate();

      var tries = 0;
      while (++tries < 20) {
        fig = c1.figures();
        if (fig['dead']['deletion'] === n && fig['alive']['count'] === 0) {
          break;
        }
        internal.wait(1);
      }

      assertEqual(0, c1.count());
      assertEqual(0, fig['alive']['count']);
      assertEqual(0, fig['alive']['size']);
      assertEqual(n, fig['dead']['count']);
      assertTrue(fig['dead']['size'] > 0);
      assertEqual(n, fig['dead']['deletion']);
      assertEqual(0, fig['journals']['count']);
      assertTrue(fig['datafiles']['count'] > 0);

      // wait for compactor to run
      require('console').log('waiting for compactor to run');

      // set max wait time
      if (internal.valgrind) {
        maxWait = 120;
      } else {
        maxWait = 15;
      }

      tries = 0;
      while (++tries < maxWait) {
        fig = c1.figures();
        if (fig['alive']['count'] === 0) {
          break;
        }
        internal.wait(1, false);
      }

      assertEqual(0, c1.count());
      assertEqual(0, fig['alive']['count']);
      assertEqual(0, fig['alive']['size']);
      assertEqual(n, fig['dead']['count']);
      assertTrue(fig['dead']['size'] > 0);
      assertEqual(n, fig['dead']['deletion']);
      assertEqual(0, fig['journals']['count']);
      assertTrue(fig['datafiles']['count'] > 0);

      c1.save({
        'some data': true
      });

      fig = c1.figures();
      assertEqual(0, fig['journals']['count']);

      internal.wal.flush(true, true);

      tries = 0;
      while (++tries < maxWait) {
        fig = c1.figures();
        if (fig['journals']['count'] === 1) {
          break;
        }
        internal.wait(1);
      }
      assertEqual(1, fig['journals']['count']);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test document presence after compaction
    // //////////////////////////////////////////////////////////////////////////////

    testDocumentPresence: function () {
      var maxWait;
      var waited;
      var n = 20000;
      var i;
      var payload = 'the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox';

      for (i = 0; i < 5; ++i) {
        payload += payload;
      }

      var c1 = internal.db._create(cn, {
        'journalSize': 1048576
      });
      const initialFigures = c1.figures();

      for (i = 0; i < n; ++i) {
        c1.save({
          _key: 'test' + i,
          value: i,
          payload: payload
        });
      }

      for (i = 0; i < n; i += 2) {
        c1.remove('test' + i);
      }

      internal.wal.flush(true, true);
      // this will create a barrier that will block compaction
      c1.document('test1');

      c1.rotate();

      var tries = 0;
      var fig;

      console.log("waiting for alive.count === " + n/2 + " and dead.count === 0");
      while (++tries < 500) {
        fig = c1.figures();
        if (fig['alive']['count'] === n / 2 && fig['dead']['count'] === 0) {
          break;
        }
        internal.wait(1, false);
      }

      assertEqual(n / 2, c1.count());
      assertEqual(n / 2, fig['alive']['count']);
      assertEqual(0, fig['dead']['count']);
      assertEqual(0, fig['dead']['size']);
      assertTrue(200, fig['dead']['deletion']);
      assertTrue(fig['journals']['count'] >= 0);
      assertTrue(fig['datafiles']['count'] > 0);

      // trigger GC
      internal.wait(0);

      // wait for compactor to run
      if (waitForCompaction(c1, initialFigures, 100, 1) === false) {
        throw "No compaction occurred!";
      }

      for (i = 0; i < n; i++) {
        // will throw if document does not exist
        if (i % 2 === 0) {
          try {
            c1.document('test' + i);
            fail();
          } catch (err) {
          }
        } else {
          c1.document('test' + i);
        }
      }

      // trigger GC
      internal.wait(0);

      c1.truncate();
      internal.wal.flush(true, true);
      c1.rotate();

      waited = 0;

      maxWait = 10000;
      console.log("Waiting for dead.deletion === 0 and dead.count === 0");
      while (waited < maxWait) {
        internal.wait(2, false);
        waited += 2;

        fig = c1.figures();
        if (fig['dead']['deletion'] === 0 && fig['dead']['count'] === 0) {
          break;
        }
      }

      fig = c1.figures();
      assertEqual(0, c1.count(), fig);
      assertEqual(0, fig['alive']['count'], fig);
      assertEqual(0, fig['dead']['count'], fig);
      assertEqual(0, fig['dead']['size'], fig);
      assertEqual(0, fig['dead']['deletion'], fig);
      assertEqual(0, fig['journals']['count'], fig);
      assertTrue(fig['datafiles']['count'] <= 1, fig);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief creates documents, rotates the journal and truncates all documents
    // /
    // / this will fully compact the 1st datafile (with the data), but will leave
    // / the journal with the delete markers untouched
    // //////////////////////////////////////////////////////////////////////////////

    testCompactAfterTruncate: function () {
      var maxWait;
      var n = 400;
      var i;
      var payload = 'the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox';

      for (i = 0; i < 5; ++i) {
        payload += payload;
      }

      var c1 = internal.db._create(cn, {
        'journalSize': 1048576
      });
      const initialFigures = c1.figures();

      for (i = 0; i < n; ++i) {
        c1.save({ value: i,
                  payload: payload
                });
      }

      internal.wal.flush(true, true);

      var tries = 0;
      var fig;
      while (++tries < 20) {
        fig = c1.figures();
        if (fig['alive']['count'] === n) {
          break;
        }
        internal.wait(1, false);
      }

      assertEqual(n, c1.count());
      assertEqual(n, fig['alive']['count'], fig);
      assertEqual(0, fig['dead']['count'], fig);
      assertEqual(0, fig['dead']['deletion'], fig);
      assertEqual(1, fig['journals']['count'], fig);
      assertTrue(fig['datafiles']['count'] > 0, fig);

      // truncation will go fully into the journal...
      internal.wal.flush(true, true);
      c1.rotate();

      c1.truncate();

      internal.wal.flush(true, true);

      tries = 0;
      while (++tries < 20) {
        fig = c1.figures();

        if (fig['alive']['count'] === 0 && fig['dead']['deletion'] === n) {
          break;
        }
        internal.wait(1, false);
      }

      assertEqual(0, c1.count(), fig);
      assertEqual(0, fig['alive']['count'], fig);
      assertEqual(n, fig['dead']['deletion'], fig);

      if (waitForCompaction(c1, initialFigures, 100, 1) === false) {
        throw "No compaction occurred!";
      }

      fig = c1.figures();

      assertEqual(0, c1.count(), fig);
      // all alive & dead markers should be gone
      assertEqual(0, fig['alive']['count'], fig);
      assertEqual(0, fig['dead']['count'], fig);
      // we should still have all the deletion markers
      assertTrue(n >= fig['dead']['deletion'], fig);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test document presence after compaction
    // //////////////////////////////////////////////////////////////////////////////

    testCompactionAfterUpdateSingleDatafile: function () {
      var n = 2000;
      var i;
      var fig;
      var payload = 'the quick brown fox jumped over the lazy dog.';

      var c1 = internal.db._create(cn, {
        'journalSize': 1048576
      });

      for (i = 0; i < n; ++i) {
        c1.save({
          _key: 'test' + i,
          value: i,
          payload: payload
        });
      }

      fig = c1.figures();
      assertEqual(0, fig.dead.count);

      internal.wal.flush(true, true);
      // wait for the above docs to get into the collection journal
      internal.wal.waitForCollector(cn);
      internal.wait(0.5);

      // update all documents so the existing revisions become irrelevant
      for (i = 0; i < n; ++i) {
        c1.update('test' + i, {
          payload: payload + ', isn\'t that nice?',
          updated: true
        });
      }

      fig = c1.figures();
      assertTrue(fig.dead.count > 0, fig);

      internal.wal.flush(true, true);
      internal.wal.waitForCollector(cn);

      // wait for the compactor. it shouldn't compact now, but give it a chance to run
      internal.wait(5);

      // unload collection
      c1.unload();
      c1 = null;

      while (internal.db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.db._collection(cn).unload();
        internal.wait(1, false);
      }

      // now reload and check documents
      c1 = internal.db._collection(cn);
      c1.load();

      for (i = 0; i < n; ++i) {
        var doc = c1.document('test' + i);
        assertEqual(payload + ', isn\'t that nice?', doc.payload);
        assertTrue(doc.updated);
      }
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test document presence after compaction
    // //////////////////////////////////////////////////////////////////////////////

    testCompactionAfterUpdateMultipleDatafiles: function () {
      var n = 2000;
      var i;
      var fig;
      var payload = 'the quick brown fox jumped over the lazy dog.';

      var c1 = internal.db._create(cn, {
        'journalSize': 1048576
      });

      for (i = 0; i < n; ++i) {
        c1.save({
          _key: 'test' + i,
          value: i,
          payload: payload
        });
      }

      fig = c1.figures();
      assertEqual(0, fig.dead.count);

      internal.wal.flush(true, true);
      // wait for the above docs to get into the collection journal
      internal.wal.waitForCollector(cn);
      internal.wait(0.5);

      // update all documents so the existing revisions become irrelevant
      for (i = 0; i < n; ++i) {
        c1.update('test' + i, {
          payload: payload + ', isn\'t that nice?',
          updated: true
        });
      }

      fig = c1.figures();
      assertTrue(fig.dead.count > 0);

      // create new datafile
      c1.rotate();

      internal.wal.flush(true, true);
      internal.wal.waitForCollector(cn);

      // wait for the compactor. it should compact now
      var tries = 0;
      while (tries++ < 20) {
        fig = c1.figures();
        if (fig.dead.count === 0) {
          break;
        }
        internal.wait(1);
      }

      assertEqual(0, fig.dead.count);

      // unload collection
      c1.unload();
      c1 = null;

      while (internal.db._collection(cn).status() !== ArangoCollection.STATUS_UNLOADED) {
        internal.db._collection(cn).unload();
        internal.wait(1, false);
      }

      // now reload and check documents
      c1 = internal.db._collection(cn);
      c1.load();

      for (i = 0; i < n; ++i) {
        var doc = c1.document('test' + i);
        assertEqual(payload + ', isn\'t that nice?', doc.payload);
        assertTrue(doc.updated);
      }

      assertEqual(0, c1.figures().dead.count);
    },

    testCompactionRunsAndStatistics: function () {
      var payload = 'the quick brown fox jumped over the lazy dog.';

      var i = 0;
      for (i = 0; i < 5; i++) payload += payload;

      var c1 = internal.db._create(cn, {
        'journalSize': 1048576
      });

      internal.db._query(
        'FOR i IN 1..50000 INSERT { _key: CONCAT("test", i), name: CONCAT("test", i), foo: @payload } IN @@col',
        {
          payload: payload,
          '@col': cn
        });

      var fig = c1.figures();

      assertEqual(fig.dead.count, 0);

      internal.wal.flush(true, true);

      assertEqual(fig.compactionStatus.count, 0);
      assertEqual(fig.compactionStatus.bytesRead, 0);
      assertEqual(fig.compactionStatus.bytesWritten, 0);

      internal.db._query(
        'FOR i IN 700..800   REMOVE { _key: CONCAT("test", i) } IN @@col',
        {
          '@col': cn
        });

      internal.wal.waitForCollector(cn);
      internal.wait(0.5);
      i = 0;
      while (c1.figures().compactionStatus.count === fig.compactionStatus.count) {
        internal.wait(0.5);
        i += 1;
        assertTrue(i < 50);
      }
      // We should have had a compactor run by now:
      assertEqual(c1.figures().compactionStatus.count, fig.compactionStatus.count + 1);

      // It should have erased something, so the values should have increased:
      assertTrue(c1.figures().compactionStatus.bytesRead > fig.compactionStatus.bytesRead);
      assertTrue(c1.figures().compactionStatus.bytesWritten > fig.compactionStatus.bytesWritten);

      internal.wal.waitForCollector(cn);

      assertEqual(c1.figures().compactionStatus.count, fig.compactionStatus.count + 1);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suites
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(CompactionSuite);

return jsunity.done();

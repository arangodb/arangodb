/* jshint globalstrict:false, strict:false */
/* global assertEqual, assertNotNull, assertNull, assertTrue, assertMatch,
  assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the graph class
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the 'License');
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an 'AS IS' BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler, Lucas Dohmen
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');

var db = require('@arangodb').db;
var SimpleQueryArray = require('@arangodb/simple-query').SimpleQueryArray;

// //////////////////////////////////////////////////////////////////////////////
// / @brief test lookup-by-key API
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryLookupByKeysSuite () {
  'use strict';

  var cn = 'SimpleQueryLookupByKeysSuite';
  var c = null;

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop(cn);
      c = null;
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in empty collection
// //////////////////////////////////////////////////////////////////////////////

    testEmptyCollection: function () {
      var result = c.documents([ 'foo', 'bar', 'baz' ]);

      assertEqual({ documents: [ ] }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in empty collection and empty lookup list
// //////////////////////////////////////////////////////////////////////////////

    testEmptyCollectionAndArray: function () {
      var result = c.documents([ ]);

      assertEqual({ documents: [ ] }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in collection with empty lookup list
// //////////////////////////////////////////////////////////////////////////////

    testEmptyArray: function () {
      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: 'test' + i });
      }

      var result = c.documents([ ]);

      assertEqual({ documents: [ ] }, result);

      // try with alias method
      result = c.lookupByKeys([ ]);
      assertEqual({ documents: [ ] }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in collection with document ids
// //////////////////////////////////////////////////////////////////////////////

    testDocumentIds: function () {
      var keys = [ ];

      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: 'test' + i });
        keys.push(c.name() + '/test' + i);
      }

      // should have matches
      var result = c.documents(keys);
      assertEqual(100, result.documents.length);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in collection with document ids
// //////////////////////////////////////////////////////////////////////////////

    testDocumentIdDuplicates: function () {
      var keys = [ ];

      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: 'test' + i });
        // each document is here exactly twice
        keys.push(c.name() + '/test' + i);
        keys.push(c.name() + '/test' + i);
      }

      // should have matches
      var result = c.documents(keys);
      assertEqual(100, result.documents.length);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in collection with document ids
// //////////////////////////////////////////////////////////////////////////////

    testDocumentIdNonExisting: function () {
      var keys = [ ];

      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: 'test' + i });
        keys.push(c.name() + '/foo' + i);
      }

      // should not have matches
      var result = c.documents(keys);
      assertEqual(0, result.documents.length);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in collection with document ids
// //////////////////////////////////////////////////////////////////////////////

    testDocumentIdsInvalid: function () {
      var keys = [ ];

      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: 'test' + i });
        keys.push(c.name() + '/');
      }

      // should not have matches
      var result = c.documents(keys);
      assertEqual(0, result.documents.length);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in collection with document ids
// //////////////////////////////////////////////////////////////////////////////

    testDocumentIdsOtherCollection: function () {
      var keys = [ ];

      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: 'test' + i });
        keys.push('Another' + c.name() + '/test' + i);
      }

      // should not have matches
      var result = c.documents(keys);
      assertEqual(0, result.documents.length);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in collection with numeric keys
// //////////////////////////////////////////////////////////////////////////////

    testNumericKeys: function () {
      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: String(i) });
      }

      // should have matches
      var result = c.documents([ '1', '2', '3', '0' ]);
      assertEqual(4, result.documents.length);

      // try with alias method
      result = c.lookupByKeys([ '1', '2', '3', '0' ]);
      assertEqual(4, result.documents.length);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup using invalid keys
// //////////////////////////////////////////////////////////////////////////////

    testInvalidKeys: function () {
      var result = c.documents([ ' ', '*  ', ' bfffff/\\&, ', '////.,;::' ]);

      assertEqual({ documents: [ ] }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in collection
// //////////////////////////////////////////////////////////////////////////////

    testLookup: function () {
      var keys = [ ];
      for (var i = 0; i < 1000; ++i) {
        c.insert({ _key: 'test' + i, value: i });
        keys.push('test' + i);
      }

      var result = c.documents(keys);
      assertEqual(1000, result.documents.length);

      result.documents.forEach(function (doc) {
        assertTrue(doc.hasOwnProperty('_id'));
        assertTrue(doc.hasOwnProperty('_key'));
        assertTrue(doc.hasOwnProperty('_rev'));
        assertTrue(doc.hasOwnProperty('value'));
        assertMatch(/^test\d+$/, doc._key);
        assertEqual(cn + '/' + doc._key, doc._id);
        assertEqual(doc._key, 'test' + doc.value);
      });

      // try with alias method
      result = c.lookupByKeys(keys);
      assertEqual(1000, result.documents.length);

      result.documents.forEach(function (doc) {
        assertTrue(doc.hasOwnProperty('_id'));
        assertTrue(doc.hasOwnProperty('_key'));
        assertTrue(doc.hasOwnProperty('_rev'));
        assertTrue(doc.hasOwnProperty('value'));
        assertMatch(/^test\d+$/, doc._key);
        assertEqual(cn + '/' + doc._key, doc._id);
        assertEqual(doc._key, 'test' + doc.value);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup in collection
// //////////////////////////////////////////////////////////////////////////////

    testLookupDuplicate: function () {
      var keys = [ ];
      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: 'test' + i, value: i });
        // add same key twice
        keys.push('test' + i);
        keys.push('test' + i);
      }

      var result = c.documents(keys);
      assertEqual(100, result.documents.length);

      result.documents.forEach(function (doc) {
        assertTrue(doc.hasOwnProperty('_id'));
        assertTrue(doc.hasOwnProperty('_key'));
        assertTrue(doc.hasOwnProperty('_rev'));
        assertTrue(doc.hasOwnProperty('value'));
        assertMatch(/^test\d+$/, doc._key);
        assertEqual(cn + '/' + doc._key, doc._id);
        assertEqual(doc._key, 'test' + doc.value);
      });
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test remove-by-keys API
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryRemoveByKeysSuite () {
  'use strict';

  var cn = 'SimpleQueryRemoveByKeysSuite';
  var c = null;

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      db._drop(cn);
      c = null;
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in empty collection
// //////////////////////////////////////////////////////////////////////////////

    testRemoveEmptyCollection: function () {
      var result = c.removeByKeys([ 'foo', 'bar', 'baz' ]);

      assertEqual({ removed: 0, ignored: 3 }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in empty collection and empty lookup list
// //////////////////////////////////////////////////////////////////////////////

    testRemoveEmptyCollectionAndArray: function () {
      var result = c.removeByKeys([ ]);

      assertEqual({ removed: 0, ignored: 0 }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in collection with empty lookup list
// //////////////////////////////////////////////////////////////////////////////

    testRemoveEmptyArray: function () {
      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: 'test' + i });
      }

      var result = c.removeByKeys([ ]);

      assertEqual({ removed: 0, ignored: 0 }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in collection with existing and nonexisting keys
// //////////////////////////////////////////////////////////////////////////////

    testRemoveMixed: function () {
      var keys = [ ];
      for (var i = 0; i < 500; ++i) {
        c.insert({ _key: 'test' + i });

        if (i % 2 === 0) {
          keys.push('test' + i);
        } else {
          keys.push('foobar' + i);
        }
      }

      var result = c.removeByKeys(keys);

      assertEqual({ removed: 250, ignored: 250 }, result);

      assertEqual(250, c.count());
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in collection with nonexisting keys
// //////////////////////////////////////////////////////////////////////////////

    testRemoveNonExisting: function () {
      var keys = [ ];
      for (var i = 0; i < 100; ++i) {
        keys.push('test' + i);
      }

      var result = c.removeByKeys(keys);

      assertEqual({ removed: 0, ignored: 100 }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in collection with numeric keys
// //////////////////////////////////////////////////////////////////////////////

    testRemoveNumericKeys: function () {
      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: String(i) });
      }

      // should have matches
      var result = c.removeByKeys([ '1', '2', '3', '0' ]);
      assertEqual({ removed: 4, ignored: 0 }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove using invalid keys
// //////////////////////////////////////////////////////////////////////////////

    testRemoveInvalidKeys: function () {
      var result = c.removeByKeys([ ' ', '*  ', ' bfffff/\\&, ', '////.,;::' ]);

      assertEqual({ removed: 0, ignored: 4 }, result);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in collection
// //////////////////////////////////////////////////////////////////////////////

    testRemove: function () {
      var keys = [ ];
      for (var i = 0; i < 1000; ++i) {
        c.insert({ _key: 'test' + i, value: i });
        keys.push('test' + i);
      }

      var result = c.removeByKeys(keys);
      assertEqual({ removed: 1000, ignored: 0 }, result);

      assertEqual(0, c.count());
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in collection
// //////////////////////////////////////////////////////////////////////////////

    testRemoveTwice: function () {
      var keys = [ ];
      for (var i = 0; i < 1000; ++i) {
        c.insert({ _key: 'test' + i, value: i });
        keys.push('test' + i);
      }

      var result = c.removeByKeys(keys);
      assertEqual({ removed: 1000, ignored: 0 }, result);

      assertEqual(0, c.count());
      c.insert({ _key: 'test3' });
      c.insert({ _key: 'test1000' });

      result = c.removeByKeys(keys);
      assertEqual({ removed: 1, ignored: 999 }, result);

      assertEqual(1, c.count());
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in collection
// //////////////////////////////////////////////////////////////////////////////

    testRemoveDuplicate: function () {
      var keys = [ ];
      for (var i = 0; i < 100; ++i) {
        c.insert({ _key: 'test' + i, value: i });
        // add same key twice
        keys.push('test' + i);
        keys.push('test' + i);
      }

      var result = c.removeByKeys(keys);
      assertEqual({ removed: 100, ignored: 100 }, result);

      assertEqual(0, c.count());
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove in collection
// //////////////////////////////////////////////////////////////////////////////

    testRemovePartial: function () {
      var keys = [ ];
      for (var i = 0; i < 2000; ++i) {
        c.insert({ _key: 'test' + i, value: i });

        if (i % 2 === 0) {
          keys.push('test' + i);
        }
      }

      // result should have been de-duplicated?
      var result = c.removeByKeys(keys);
      assertEqual({ removed: 1000, ignored: 0 }, result);

      assertEqual(1000, c.count());
      assertFalse(c.exists('test0'));
      assertTrue(c.exists('test1'));
      assertFalse(c.exists('test2'));
      assertTrue(c.exists('test3'));
      // ...
      assertFalse(c.exists('test1998'));
      assertTrue(c.exists('test1999'));
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: skip and limit with a collection
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryArraySkipLimitSuite () {
  'use strict';
  var numbers = null;
  var query = null;

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      numbers = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
      query = new SimpleQueryArray(numbers);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: skip
// //////////////////////////////////////////////////////////////////////////////

    testSkip: function () {
      var n = query.clone().skip(0).toArray();

      assertEqual(n, numbers);

      n = query.clone().skip(1).toArray();

      assertEqual(n, numbers.slice(1, 10));

      n = query.clone().skip(2).toArray();

      assertEqual(n, numbers.slice(2, 10));

      n = query.clone().skip(1).skip(1).toArray();

      assertEqual(n, numbers.slice(2, 10));

      n = query.clone().skip(10).toArray();

      assertEqual(n, []);

      n = query.clone().skip(11).toArray();

      assertEqual(n, []);

      n = query.clone().skip(9).toArray();

      assertEqual(n, [numbers[9]]);

      n = query.clone().skip(9).skip(1).toArray();

      assertEqual(n, []);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: limit
// //////////////////////////////////////////////////////////////////////////////

    testLimit: function () {
      var n = query.clone().limit(10).toArray();

      assertEqual(n, numbers);

      n = query.clone().limit(9).toArray();

      assertEqual(n, numbers.slice(0, 9));

      n = query.clone().limit(9).limit(8).limit(7).toArray();

      assertEqual(n, numbers.slice(0, 7));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: skip and limit
// //////////////////////////////////////////////////////////////////////////////

    testSkipLimit: function () {
      var n = query.clone().skip(0).limit(10).toArray();

      assertEqual(n, numbers);

      n = query.clone().limit(10).skip(0).toArray();

      assertEqual(n, numbers);

      n = query.clone().limit(9).skip(1).toArray();

      assertEqual(n, numbers.slice(1, 9));

      n = query.clone().limit(9).skip(1).limit(7).toArray();

      assertEqual(n, numbers.slice(1, 8));

      n = query.clone().skip(-5).limit(3).toArray();

      assertEqual(n, numbers.slice(5, 8));

      n = query.clone().skip(-8).limit(7).skip(1).limit(4).toArray();

      assertEqual(n, numbers.slice(3, 7));

      n = query.clone().skip(-10).limit(9).skip(1).limit(7).toArray();

      assertEqual(n, numbers.slice(1, 8));
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: skip and limit with a collection
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryAllSkipLimitSuite () {
  'use strict';
  var cn = 'UnitTestsCollectionSkipLimit';
  var collection = null;
  var numbers = null;
  var num = function (d) { return d.n; };

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      collection = db._create(cn, { waitForSync: false });

      for (var i = 0; i < 10; ++i) {
        collection.save({ n: i });
      }

      numbers = collection.all().toArray().map(num);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      collection.drop();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: skip
// //////////////////////////////////////////////////////////////////////////////

    testAllSkip: function () {
      var n = collection.all().skip(0).toArray().map(num);
      assertEqual(n, numbers);

      n = collection.all().skip(1).toArray().map(num);
      assertEqual(n, numbers.slice(1, 10));

      n = collection.all().skip(2).toArray().map(num);
      assertEqual(n, numbers.slice(2, 10));

      n = collection.all().skip(1).skip(1).toArray().map(num);
      assertEqual(n, numbers.slice(2, 10));

      n = collection.all().skip(10).toArray().map(num);
      assertEqual(n, []);

      n = collection.all().skip(11).toArray().map(num);
      assertEqual(n, []);

      n = collection.all().skip(9).toArray().map(num);
      assertEqual(n, [numbers[9]]);

      n = collection.all().skip(9).skip(1).toArray().map(num);
      assertEqual(n, []);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: limit
// //////////////////////////////////////////////////////////////////////////////

    testAllLimit: function () {
      var n = collection.all().limit(10).toArray().map(num);
      assertEqual(n, numbers);

      n = collection.all().limit(9).toArray().map(num);
      assertEqual(n, numbers.slice(0, 9));

      n = collection.all().limit(9).limit(8).limit(7).toArray().map(num);
      assertEqual(n, numbers.slice(0, 7));
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: skip and limit
// //////////////////////////////////////////////////////////////////////////////

    testAllSkipLimit: function () {
      var n = collection.all().skip(0).limit(10).toArray().map(num);
      assertEqual(n, numbers);

      n = collection.all().limit(10).skip(0).toArray().map(num);
      assertEqual(n, numbers);

      n = collection.all().limit(9).skip(1).toArray().map(num);
      assertEqual(n, numbers.slice(1, 9));

      n = collection.all().limit(9).skip(1).limit(7).toArray().map(num);
      assertEqual(n, numbers.slice(1, 8));
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: query-by-example
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryByExampleSuite () {
  'use strict';
  var cn = 'UnitTestsCollectionByExample';
  var collection = null;
  var id = function (d) { return d._id; };

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      collection = db._create(cn);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      collection.drop();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using indexes
// //////////////////////////////////////////////////////////////////////////////

    testByExampleHash1: function () {
      var d, s;
      var example = { 'a': { 'b': true, 'c': 'foo' }, 'd': true };

      collection.ensureHashIndex('d');

      d = collection.save(example);
      s = collection.firstExample({ 'd': true });
      assertEqual(d._id, s._id);

      s = collection.firstExample({ 'd': false });
      assertNull(s);

      s = collection.firstExample({ 'a.b': true });
      assertEqual(d._id, s._id);

      s = collection.firstExample({ 'a.c': 'foo' });
      assertEqual(d._id, s._id);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using indexes
// //////////////////////////////////////////////////////////////////////////////

    testByExampleHash2: function () {
      var d1, d2, d4, s;
      var example1 = { 'a': { 'b': true } };
      var example2 = { 'a': { 'b': 'foo' } };
      var example3 = { 'a': { 'b': null } };
      var example4 = { 'c': 1 };

      collection.ensureHashIndex('a.b');

      d1 = collection.save(example1);
      d2 = collection.save(example2);
      collection.save(example3);
      d4 = collection.save(example4);

      s = collection.firstExample({ 'a.b': true });
      assertEqual(d1._id, s._id);
      s = collection.firstExample({ 'a.b': 'foo' });
      assertEqual(d2._id, s._id);
      s = collection.firstExample({ 'a.b': false });
      assertNull(s);
      s = collection.firstExample({ 'a.b': null });
      assertNotNull(s);
      s = collection.firstExample({ 'c': 1 });
      assertEqual(d4._id, s._id);

      s = collection.firstExample(example1);
      assertEqual(d1._id, s._id);
      s = collection.firstExample(example2);
      assertEqual(d2._id, s._id);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using indexes
// //////////////////////////////////////////////////////////////////////////////

    testByExampleSparseHash: function () {
      var d1, d2, d4, s;
      var example1 = { 'a': { 'b': true } };
      var example2 = { 'a': { 'b': 'foo' } };
      var example3 = { 'a': { 'b': null } };
      var example4 = { 'c': 1 };

      collection.ensureHashIndex('a.b', { sparse: true });

      d1 = collection.save(example1);
      d2 = collection.save(example2);
      collection.save(example3);
      d4 = collection.save(example4);

      s = collection.firstExample({ 'a.b': true });
      assertEqual(d1._id, s._id);
      s = collection.firstExample({ 'a.b': 'foo' });
      assertEqual(d2._id, s._id);
      s = collection.firstExample({ 'a.b': false });
      assertNull(s);
      s = collection.firstExample({ 'a.b': null });
      assertNotNull(s);
      s = collection.firstExample({ 'c': 1 });
      assertEqual(d4._id, s._id);

      s = collection.firstExample(example1);
      assertEqual(d1._id, s._id);
      s = collection.firstExample(example2);
      assertEqual(d2._id, s._id);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using indexes
// //////////////////////////////////////////////////////////////////////////////

    testByExampleSkiplist1: function () {
      var d, s;
      var example = { 'a': { 'b': true, 'c': 'foo' }, 'd': true };

      collection.ensureSkiplist('d');

      d = collection.save(example);
      s = collection.firstExample({ 'd': true });
      assertEqual(d._id, s._id);

      s = collection.firstExample({ 'd': false });
      assertNull(s);

      s = collection.firstExample({ 'a.b': true });
      assertEqual(d._id, s._id);

      s = collection.firstExample({ 'a.c': 'foo' });
      assertEqual(d._id, s._id);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using indexes
// //////////////////////////////////////////////////////////////////////////////

    testByExampleSkiplist2: function () {
      var d1, d2, d4, s;
      var example1 = { 'a': { 'b': true } };
      var example2 = { 'a': { 'b': 'foo' } };
      var example3 = { 'a': { 'b': null } };
      var example4 = { 'c': 1 };

      collection.ensureSkiplist('a.b');

      d1 = collection.save(example1);
      d2 = collection.save(example2);
      collection.save(example3);
      d4 = collection.save(example4);

      s = collection.firstExample({ 'a.b': true });
      assertEqual(d1._id, s._id);
      s = collection.firstExample({ 'a.b': 'foo' });
      assertEqual(d2._id, s._id);
      s = collection.firstExample({ 'a.b': false });
      assertNull(s);
      s = collection.firstExample({ 'a.b': null });
      assertNotNull(s);
      s = collection.firstExample({ 'c': 1 });
      assertEqual(d4._id, s._id);

      s = collection.firstExample(example1);
      assertEqual(d1._id, s._id);
      s = collection.firstExample(example2);
      assertEqual(d2._id, s._id);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using indexes
// //////////////////////////////////////////////////////////////////////////////

    testByExampleSparseSkiplist: function () {
      var d1, d2, d4, s;
      var example1 = { 'a': { 'b': true } };
      var example2 = { 'a': { 'b': 'foo' } };
      var example3 = { 'a': { 'b': null } };
      var example4 = { 'c': 1 };

      collection.ensureSkiplist('a.b', { sparse: true });

      d1 = collection.save(example1);
      d2 = collection.save(example2);
      collection.save(example3);
      d4 = collection.save(example4);

      s = collection.firstExample({ 'a.b': true });
      assertEqual(d1._id, s._id);
      s = collection.firstExample({ 'a.b': 'foo' });
      assertEqual(d2._id, s._id);
      s = collection.firstExample({ 'a.b': false });
      assertNull(s);
      s = collection.firstExample({ 'a.b': null });
      assertNotNull(s);
      s = collection.firstExample({ 'c': 1 });
      assertEqual(d4._id, s._id);

      s = collection.firstExample(example1);
      assertEqual(d1._id, s._id);
      s = collection.firstExample(example2);
      assertEqual(d2._id, s._id);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample
// //////////////////////////////////////////////////////////////////////////////

    testByExampleMultipleValues: function () {
      [ null, 1, 2, true, false, '1', '2', 'foo', 'barbazbark', [ ] ].forEach(function (v) {
        for (var i = 0; i < 5; ++i) {
          collection.save({ value: v });
        }

        var result = collection.byExample({ value: v }).toArray();
        assertEqual(5, result.length);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample
// //////////////////////////////////////////////////////////////////////////////

    testByExampleMultipleValuesHashIndex: function () {
      collection.ensureHashIndex('value');

      [ null, 1, 2, true, false, '1', '2', 'foo', 'barbazbark', [ ] ].forEach(function (v) {
        for (var i = 0; i < 5; ++i) {
          collection.save({ value: v });
        }

        var result = collection.byExample({ value: v }).toArray();
        assertEqual(5, result.length);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample
// //////////////////////////////////////////////////////////////////////////////

    testByExampleMultipleValuesSparseHashIndex: function () {
      collection.ensureHashIndex('value', { sparse: true });

      [ null, 1, 2, true, false, '1', '2', 'foo', 'barbazbark', [ ] ].forEach(function (v) {
        for (var i = 0; i < 5; ++i) {
          collection.save({ value: v });
        }

        var result = collection.byExample({ value: v }).toArray();
        assertEqual(5, result.length);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample
// //////////////////////////////////////////////////////////////////////////////

    testByExampleMultipleValuesSkiplist: function () {
      collection.ensureSkiplist('value');

      [ null, 1, 2, true, false, '1', '2', 'foo', 'barbazbark', [ ] ].forEach(function (v) {
        for (var i = 0; i < 5; ++i) {
          collection.save({ value: v });
        }

        var result = collection.byExample({ value: v }).toArray();
        assertEqual(5, result.length);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample
// //////////////////////////////////////////////////////////////////////////////

    testByExampleMultipleValuesSparseSkiplist: function () {
      collection.ensureSkiplist('value', { sparse: true });

      [ null, 1, 2, true, false, '1', '2', 'foo', 'barbazbark', [ ] ].forEach(function (v) {
        for (var i = 0; i < 5; ++i) {
          collection.save({ value: v });
        }

        var result = collection.byExample({ value: v }).toArray();
        assertEqual(5, result.length);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using _key
// //////////////////////////////////////////////////////////////////////////////

    testByExampleKey: function () {
      var d, s;

      d = collection.save({ _key: 'meow' });
      s = collection.firstExample({ _key: 'foo' });
      assertNull(s);

      s = collection.firstExample({ _key: 'meow' });
      assertEqual(d._id, s._id);
      assertEqual(d._key, s._key);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using _key & others
// //////////////////////////////////////////////////////////////////////////////

    testByExampleKeyMore: function () {
      var d, s;

      d = collection.save({ _key: 'meow' });
      s = collection.firstExample({ _key: 'meow', a: 1 });
      assertNull(s);

      d = collection.save({ _key: 'foo', a: 2 });
      s = collection.firstExample({ _key: 'foo', a: 2 });
      assertEqual(d._id, s._id);
      assertEqual(d._key, s._key);
      assertEqual(2, s.a);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using _rev
// //////////////////////////////////////////////////////////////////////////////

    testByExampleRev: function () {
      var d, s;

      d = collection.save({ _key: 'meow' });
      s = collection.firstExample({ _rev: d._rev });
      assertEqual(d._rev, s._rev);

      s = collection.firstExample({ _rev: 'foo' });
      assertNull(s);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using _rev
// //////////////////////////////////////////////////////////////////////////////

    testByExampleRevId: function () {
      var d, s;

      d = collection.save({ _key: 'meow' });
      s = collection.firstExample({ _rev: d._rev, _id: cn + '/meow' });
      assertEqual(d._rev, s._rev);

      s = collection.firstExample({ _rev: d._rev, _id: cn + '/foo' });
      assertNull(s);

      s = collection.firstExample({ _rev: 'foo', _id: cn + '/meow' });
      assertNull(s);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using _rev
// //////////////////////////////////////////////////////////////////////////////

    testByExampleRevMore: function () {
      var d, s;

      d = collection.save({ _key: 'meow', a: 1, b: 2 });
      s = collection.firstExample({ _rev: d._rev, b: 2, a: 1 });
      assertEqual(d._rev, s._rev);
      assertEqual('meow', s._key);

      s = collection.firstExample({ _rev: d._rev, a: 1 });
      assertEqual(d._rev, s._rev);
      assertEqual('meow', s._key);

      s = collection.firstExample({ _rev: d._rev, b: 2 });
      assertEqual(d._rev, s._rev);
      assertEqual('meow', s._key);

      s = collection.firstExample({ _rev: d._rev, a: 2 });
      assertNull(s);

      s = collection.firstExample({ _rev: d._rev, b: 1 });
      assertNull(s);

      s = collection.firstExample({ _rev: d._rev, a: 1, b: 3 });
      assertNull(s);

      s = collection.firstExample({ _rev: 'foo', a: 1 });
      assertNull(s);

      s = collection.firstExample({ _rev: 'foo', b: 2 });
      assertNull(s);

      s = collection.firstExample({ _rev: 'foo', a: 1, b: 2 });
      assertNull(s);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using _id
// //////////////////////////////////////////////////////////////////////////////

    testByExampleId: function () {
      var d, s;

      d = collection.save({ _key: 'meow' });
      s = collection.firstExample({ _id: cn + '/foo' });
      assertNull(s);

      s = collection.firstExample({ _id: cn + '/meow' });
      assertEqual(d._id, s._id);
      assertEqual(d._key, s._key);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using _id & others
// //////////////////////////////////////////////////////////////////////////////

    testByExampleIdMore: function () {
      var d, s;

      d = collection.save({ _key: 'meow' });
      s = collection.firstExample({ _id: cn + '/meow', a: 1 });
      assertEqual(null, s);

      d = collection.save({ _key: 'foo', a: 2 });
      s = collection.firstExample({ _id: cn + '/foo', a: 2 });
      assertEqual(d._id, s._id);
      assertEqual(d._key, s._key);
      assertEqual(2, s.a);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using _id & sub-attributes
// //////////////////////////////////////////////////////////////////////////////

    testByExampleIdSubAttributes: function () {
      var d, s;

      d = collection.save({ _key: 'meow', foo: { bar: 'baz' } });
      s = collection.firstExample({ _id: cn + '/meow', foo: { bar: 'baz' } });
      assertEqual(d._id, s._id);
      assertEqual(d._key, s._key);
      assertEqual({ bar: 'baz' }, s.foo);

      d = collection.save({ _key: 'foo', values: [ { foo: 'bar', baz: 'bam' } ] });
      s = collection.firstExample({ _id: cn + '/foo', values: [ { foo: 'bar', baz: 'bam' } ] });
      assertEqual(d._id, s._id);
      assertEqual(d._key, s._key);
      assertEqual([ { foo: 'bar', baz: 'bam' } ], s.values);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using non existing system attribute
// //////////////////////////////////////////////////////////////////////////////

    testByExampleNonExisting: function () {
      var s;

      collection.save({ _key: 'meow' });
      s = collection.firstExample({ _foo: 'foo' });
      assertEqual(null, s);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample
// //////////////////////////////////////////////////////////////////////////////

    testByExampleObject: function () {
      var d1 = collection.save({ i: 1 });
      var d2 = collection.save({ i: 1, a: { j: 1 } });
      var d3 = collection.save({ i: 1, a: { j: 1, k: 1 } });
      var d4 = collection.save({ i: 1, a: { j: 2, k: 2 } });
      var d5 = collection.save({ i: 2 });
      var d6 = collection.save({ i: 2, a: 2 });
      var d7 = collection.save({ i: 2, a: { j: 2, k: 2 } });
      var s;

      s = collection.byExample({ i: 1 }).toArray().map(id).sort();
      assertEqual([d1._id, d2._id, d3._id, d4._id].sort(), s);

      s = collection.byExample({ i: 2 }).toArray().map(id).sort();
      assertEqual([d5._id, d6._id, d7._id].sort(), s);

      s = collection.byExample({ a: { j: 1 } }).toArray().map(id).sort();
      assertEqual([d2._id], s);

      s = collection.byExample({ 'a.j': 1 }).toArray().map(id).sort();
      assertEqual([d2._id, d3._id].sort(), s);

      s = collection.byExample({ i: 2, 'a.k': 2 }).toArray().map(id).sort();
      assertEqual([d7._id], s);

      s = collection.firstExample({ 'i': 2, 'a.k': 2 });
      assertEqual(d7._id, s._id);

      s = collection.firstExample({ 'i': 2, 'a.k': 27 });
      assertEqual(null, s);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample
// //////////////////////////////////////////////////////////////////////////////

    testByExampleList: function () {
      var d1 = collection.save({ i: 1 });
      var d2 = collection.save({ i: 1, a: { j: 1 } });
      var d3 = collection.save({ i: 1, a: { j: 1, k: 1 } });
      var d4 = collection.save({ i: 1, a: { j: 2, k: 2 } });
      var d5 = collection.save({ i: 2 });
      var d6 = collection.save({ i: 2, a: 2 });
      var d7 = collection.save({ i: 2, a: { j: 2, k: 2 } });
      var s;

      s = collection.byExample('i', 1).toArray().map(id).sort();
      assertEqual([d1._id, d2._id, d3._id, d4._id].sort(), s);

      s = collection.byExample('i', 2).toArray().map(id).sort();
      assertEqual([d5._id, d6._id, d7._id].sort(), s);

      s = collection.byExample('a', { j: 1 }).toArray().map(id).sort();
      assertEqual([d2._id], s);

      s = collection.byExample('a.j', 1).toArray().map(id).sort();
      assertEqual([d2._id, d3._id].sort(), s);

      s = collection.byExample('i', 2, 'a.k', 2).toArray().map(id).sort();
      assertEqual([d7._id], s);

      s = collection.firstExample('i', 2, 'a.k', 2);
      assertEqual(d7._id, s._id);

      s = collection.firstExample('i', 2, 'a.k', 27);
      assertEqual(null, s);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: removeByExample
// //////////////////////////////////////////////////////////////////////////////

    testRemoveByExample: function () {
      var deleted;

      for (var i = 0; i < 50; ++i) {
        collection.save({ value: i });
      }

      deleted = collection.removeByExample({ value: 2 });
      assertEqual(1, deleted);

      deleted = collection.removeByExample({ value: 2 });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ value: 20 });
      assertEqual(1, deleted);

      deleted = collection.removeByExample({ value: 20 });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ value: 1 });
      assertEqual(1, deleted);

      // not existing documents
      deleted = collection.removeByExample({ value: 50 });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ value: null });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ value: '5' });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ peter: 'meow' });
      assertEqual(0, deleted);

      collection.truncate({ compact: false });
      deleted = collection.removeByExample({ value: 4 });
      assertEqual(0, deleted);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: removeByExample with new signature
// //////////////////////////////////////////////////////////////////////////////

    testRemoveByExampleWithNewSignature: function () {
      var deleted;

      collection.save({ b: 1, value: 2222 });

      for (var i = 0; i < 50; ++i) {
        collection.save({ a: 1, value: i });
      }
      // test default parameter
      deleted = collection.removeByExample({ b: 1 });
      assertEqual(1, deleted);

      deleted = collection.removeByExample({ a: 1 }, {limit: 10});
      assertEqual(10, deleted);

      deleted = collection.removeByExample({ a: 1 }, {waitForSync: true, limit: 15});
      assertEqual(15, deleted);

      // not existing documents
      deleted = collection.removeByExample({ value: 500 });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ value: null });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ value: '5' });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ peter: 'meow' });
      assertEqual(0, deleted);

      collection.truncate({ compact: false });
      deleted = collection.removeByExample({ value: 4 });
      assertEqual(0, deleted);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: removeByExample
// //////////////////////////////////////////////////////////////////////////////

    testRemoveByExampleMult: function () {
      var deleted;

      for (var i = 0; i < 5; ++i) {
        for (var j = 0; j < 5; ++j) {
          collection.save({ value1: i, value2: j });
        }
      }

      deleted = collection.removeByExample({ value2: 2 });
      assertEqual(5, deleted);

      deleted = collection.removeByExample({ value2: 2 });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ value1: 4 });
      assertEqual(4, deleted);

      deleted = collection.removeByExample({ value1: 4 });
      assertEqual(0, deleted);

      // not existing documents
      deleted = collection.removeByExample({ value1: 99 });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ value1: '0' });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ meow: 1 });
      assertEqual(0, deleted);

      deleted = collection.removeByExample({ meow: 'peter' });
      assertEqual(0, deleted);

      collection.truncate({ compact: false });
      deleted = collection.removeByExample({ value1: 3 });
      assertEqual(0, deleted);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: removeByExample
// //////////////////////////////////////////////////////////////////////////////

    testRemoveByExampleLimit: function () {
      var deleted;

      for (var i = 0; i < 50; ++i) {
        collection.save({ value: 1 });
      }

      deleted = collection.removeByExample({ value: 1 }, false, 10);
      assertEqual(10, deleted);

      assertEqual(40, collection.count());

      deleted = collection.removeByExample({ value: 1 }, false, 20);
      assertEqual(20, deleted);

      assertEqual(20, collection.count());

      deleted = collection.removeByExample({ value: 1 }, false, 20);
      assertEqual(20, deleted);

      assertEqual(0, collection.count());

      deleted = collection.removeByExample({ value: 1 }, false, 20);
      assertEqual(0, deleted);

      assertEqual(0, collection.count());
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: replaceByExample
// //////////////////////////////////////////////////////////////////////////////

    testReplaceByExample: function () {
      var replaced;

      for (var i = 0; i < 50; ++i) {
        collection.save({ value: i });
      }

      replaced = collection.replaceByExample({ value: 2 }, { foo: 'bar', bar: 'baz' });
      assertEqual(1, replaced);

      assertEqual(50, collection.count());

      var doc = collection.firstExample({ foo: 'bar', bar: 'baz' });
      assertEqual('bar', doc.foo);
      assertEqual('baz', doc.bar);
      assertEqual(undefined, doc.value);

      // not existing documents
      replaced = collection.replaceByExample({ meow: true }, { });

      assertEqual(0, replaced);

      replaced = collection.replaceByExample({ value: 142 }, { });
      assertEqual(0, replaced);

      replaced = collection.replaceByExample({ value: 'peng!' }, { });
      assertEqual(0, replaced);

      collection.truncate({ compact: false });
      replaced = collection.replaceByExample({ value: 6 }, { });
      assertEqual(0, replaced);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: replaceByExampleWithNewSignature
// //////////////////////////////////////////////////////////////////////////////

    testReplaceByExampleWithNewSignature: function () {
      var replaced;

      for (var i = 0; i < 50; ++i) {
        collection.save({ value: 2, a: i, b: i + 1 });
      }

      replaced = collection.replaceByExample({ value: 2 },
                                             { foo: 'bar', bar: 'baz' },
                                             {limit: 20, waitForSync: true});
      assertEqual(20, replaced);

      assertEqual(50, collection.count());

      var doc = collection.firstExample({ foo: 'bar', bar: 'baz' });
      assertEqual('bar', doc.foo);
      assertEqual('baz', doc.bar);
      assertEqual(undefined, doc.value);
      assertEqual(undefined, doc.a);
      assertEqual(undefined, doc.b);

      // not existing documents
      replaced = collection.replaceByExample({ meow: true }, { });
      assertEqual(0, replaced);

      replaced = collection.replaceByExample({ value: 142 }, { });
      assertEqual(0, replaced);

      replaced = collection.replaceByExample({ value: 'peng!' }, { });
      assertEqual(0, replaced);

      collection.truncate({ compact: false });
      replaced = collection.replaceByExample({ value: 6 }, { });
      assertEqual(0, replaced);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: replaceByExample
// //////////////////////////////////////////////////////////////////////////////

    testReplaceByExampleLimit: function () {
      var replaced, docs;

      for (var i = 0; i < 50; ++i) {
        collection.save({ value: 1 });
      }

      replaced = collection.replaceByExample({ value: 1 }, { foo: 'bar', bar: 'baz' }, false, 10);
      assertEqual(10, replaced);

      assertEqual(50, collection.count());

      docs = collection.byExample({ foo: 'bar', bar: 'baz' }).toArray();
      assertEqual(10, docs.length);
      docs = collection.byExample({ value: 1 }).toArray();
      assertEqual(40, docs.length);

      replaced = collection.replaceByExample({ value: 1 }, { meow: false }, false, 15);
      assertEqual(15, replaced);

      assertEqual(50, collection.count());

      docs = collection.byExample({ foo: 'bar', bar: 'baz' }).toArray();
      assertEqual(10, docs.length);
      docs = collection.byExample({ meow: false }).toArray();
      assertEqual(15, docs.length);
      docs = collection.byExample({ value: 1 }).toArray();
      assertEqual(25, docs.length);

      // not existing documents
      replaced = collection.replaceByExample({ meow: true }, { }, false, 99);
      assertEqual(0, replaced);

      replaced = collection.replaceByExample({ value: 42 }, { }, false, 99);
      assertEqual(0, replaced);

      replaced = collection.replaceByExample({ value: 'peng!' }, { }, false, 99);
      assertEqual(0, replaced);

      // check counts
      docs = collection.byExample({ foo: 'bar', bar: 'baz' }).toArray();
      assertEqual(10, docs.length);
      docs = collection.byExample({ meow: false }).toArray();
      assertEqual(15, docs.length);
      docs = collection.byExample({ value: 1 }).toArray();
      assertEqual(25, docs.length);

      collection.truncate({ compact: false });
      replaced = collection.replaceByExample({ value: 1 }, { }, false);
      assertEqual(0, replaced);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: updateByExample
// //////////////////////////////////////////////////////////////////////////////

    testUpdateByExample: function () {
      var updated;

      for (var i = 0; i < 50; ++i) {
        collection.save({ value: i });
      }

      // update and keep old values
      updated = collection.updateByExample({ value: 2 }, { foo: 'bar', bar: 'baz' });
      assertEqual(1, updated);

      assertEqual(50, collection.count());

      var doc = collection.firstExample({ foo: 'bar', bar: 'baz' });
      assertEqual('bar', doc.foo);
      assertEqual('baz', doc.bar);
      assertEqual(2, doc.value);

      // update and remove old values
      updated = collection.updateByExample({ value: 5 }, { foo: 'bart', bar: 'baz', value: null }, false);
      assertEqual(1, updated);

      doc = collection.firstExample({ foo: 'bart', bar: 'baz' });
      assertEqual('bart', doc.foo);
      assertEqual('baz', doc.bar);
      assertEqual(undefined, doc.value);

      // update and overwrite old values
      updated = collection.updateByExample({ value: 17 }, { foo: 'barw', bar: 'baz', value: 9 }, false);
      assertEqual(1, updated);

      doc = collection.firstExample({ foo: 'barw', bar: 'baz' });
      assertEqual('barw', doc.foo);
      assertEqual('baz', doc.bar);
      assertEqual(9, doc.value);

      // not existing documents
      updated = collection.updateByExample({ meow: true }, { });
      assertEqual(0, updated);

      updated = collection.updateByExample({ value: 142 }, { });
      assertEqual(0, updated);

      updated = collection.updateByExample({ value: 'peng!' }, { });
      assertEqual(0, updated);

      collection.truncate({ compact: false });
      updated = collection.updateByExample({ value: 6 }, { });
      assertEqual(0, updated);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: updateByExample with new Signature
// //////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleWithNewSignature: function () {
      var updated;

      for (var i = 0; i < 50; ++i) {
        collection.save({
          value: i,
          a: 1
        });
      }

      // update and keep old values
      updated = collection.updateByExample({ value: 2 }, { foo: 'bar', bar: 'baz' });
      assertEqual(1, updated);

      assertEqual(50, collection.count());

      var doc = collection.firstExample({ foo: 'bar', bar: 'baz' });
      assertEqual('bar', doc.foo);
      assertEqual('baz', doc.bar);
      assertEqual(2, doc.value);

      // update and remove old values
      updated = collection.updateByExample({ value: 5 },
                                           { foo: 'bart', bar: 'baz', value: null },
                                           {keepNull: false});
      assertEqual(1, updated);

      doc = collection.firstExample({ foo: 'bart', bar: 'baz' });
      assertEqual('bart', doc.foo);
      assertEqual('baz', doc.bar);
      assertEqual(undefined, doc.value);

      // update and overwrite old values
      updated = collection.updateByExample({ value: 17 }, { foo: 'barw', bar: 'baz', value: 9 }, {keepNull: false});
      assertEqual(1, updated);

      doc = collection.firstExample({ foo: 'barw', bar: 'baz' });
      assertEqual('barw', doc.foo);
      assertEqual('baz', doc.bar);
      assertEqual(9, doc.value);

      // update and remove old values keep null values
      updated = collection.updateByExample({ value: 6 },
                                           { foo: 'bart6', bar: 'baz6', value: null },
                                           {keepNull: true});
      assertEqual(1, updated);

      doc = collection.firstExample({ foo: 'bart6', bar: 'baz6' });
      assertEqual('bart6', doc.foo);
      assertEqual('baz6', doc.bar);
      assertEqual(null, doc.value);

      // not existing documents
      updated = collection.updateByExample({ meow: true }, { });
      assertEqual(0, updated);

      updated = collection.updateByExample({ nonExistentValue: 'foo' }, { });
      assertEqual(0, updated);

      collection.truncate({ compact: false });
      updated = collection.updateByExample({ value: 6 }, { });
      assertEqual(0, updated);

      for (i = 0; i < 50; ++i) {
        collection.save({
          test: i,
          limitTest: 1
        });
      }
      // update and remove old values keep null values
      updated = collection.updateByExample(
        { limitTest: 1 },
        { foo: 'bart', bar: 'baz', value: null },
        {keepNull: true, limit: 30});
      assertEqual(30, updated);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: updateByExample
// //////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleLimit: function () {
      var updated, docs;

      for (var i = 0; i < 50; ++i) {
        collection.save({ value: 1 });
      }

      // update some, keep old values
      updated = collection.updateByExample({ value: 1 }, { foo: 'bar', bar: 'baz' }, false, false, 10);
      assertEqual(10, updated);

      assertEqual(50, collection.count());

      docs = collection.byExample({ foo: 'bar', bar: 'baz' }).toArray();
      assertEqual(10, docs.length);
      docs = collection.byExample({ value: 1 }).toArray();
      assertEqual(50, docs.length);

      // update some others
      updated = collection.updateByExample({ value: 1 }, { meow: false }, false, false, 15);
      assertEqual(15, updated);

      assertEqual(50, collection.count());

      docs = collection.byExample({ foo: 'bar', bar: 'baz' }).toArray();
      assertEqual(10, docs.length);
      docs = collection.byExample({ meow: false }).toArray();
      assertEqual(15, docs.length);
      docs = collection.byExample({ value: 1 }).toArray();
      assertEqual(50, docs.length);

      // update some, remove old values
      updated = collection.updateByExample({ value: 1 }, { value: null, fox: true }, false, false, 5);
      assertEqual(5, updated);

      assertEqual(50, collection.count());

      docs = collection.byExample({ fox: true }).toArray();
      assertEqual(5, docs.length);
      docs = collection.byExample({ value: 1 }).toArray();
      assertEqual(45, docs.length);

      // update some, overwrite old values
      updated = collection.updateByExample({ value: 1 }, { value: 16 }, false, false, 10);
      assertEqual(10, updated);

      assertEqual(50, collection.count());

      docs = collection.byExample({ value: 16 }).toArray();
      assertEqual(10, docs.length);
      docs = collection.byExample({ fox: true }).toArray();
      assertEqual(5, docs.length);
      docs = collection.byExample({ value: 1 }).toArray();
      assertEqual(35, docs.length);

      // not existing documents
      updated = collection.updateByExample({ meow: true }, { }, false, false, 99);
      assertEqual(0, updated);

      updated = collection.updateByExample({ value: 42 }, { }, false, false, 99);
      assertEqual(0, updated);

      updated = collection.updateByExample({ value: 'peng!' }, { }, false, false, 99);
      assertEqual(0, updated);

      collection.truncate({ compact: false });
      updated = collection.updateByExample({ value: 1 }, { });
      assertEqual(0, updated);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: updateByExample, mergeObjects
// //////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleMergeObjectsSameUnspecified: function () {
      var updated;

      collection.save({ _key: 'one', value: { foo: 'test' } });

      // update don't specify mergeObjects behavior
      updated = collection.updateByExample({ value: { foo: 'test' } }, { value: { foo: 'baz' } });
      assertEqual(1, updated);

      var doc = collection.document('one');
      assertEqual({ foo: 'baz' }, doc.value);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: updateByExample, mergeObjects
// //////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleMergeObjectsUnspecified: function () {
      var updated;

      collection.save({ _key: 'one', value: { foo: 'test' } });

      // update don't specify mergeObjects behavior
      updated = collection.updateByExample({ value: { foo: 'test' } },
                                           { value: { bar: 'baz' } });
      assertEqual(1, updated);

      var doc = collection.document('one');
      assertEqual({ foo: 'test', bar: 'baz' }, doc.value);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: updateByExample, mergeObjects
// //////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleMergeObjectsTrue: function () {
      var updated;

      collection.save({ _key: 'one', value: { foo: 'test' } });

      // update don't specify mergeObjects behavior
      updated = collection.updateByExample({ value: { foo: 'test' } },
                                           { value: { bar: 'baz' } },
                                           { mergeObjects: true });
      assertEqual(1, updated);

      var doc = collection.document('one');
      assertEqual({ foo: 'test', bar: 'baz' }, doc.value);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: updateByExample, mergeObjects
// //////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleMergeObjectsFalse: function () {
      var updated;

      collection.save({ _key: 'one', value: { foo: 'test' } });

      // update don't specify mergeObjects behavior
      updated = collection.updateByExample({ value: { foo: 'test' } },
                                           { value: { bar: 'baz' } },
                                           { mergeObjects: false });
      assertEqual(1, updated);

      var doc = collection.document('one');
      assertEqual({ bar: 'baz' }, doc.value);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: updateByExample, mergeObjects
// //////////////////////////////////////////////////////////////////////////////

    testUpdateByExampleMergeObjectsEmptyObject: function () {
      var updated;

      collection.save({ _key: 'one', value: { foo: 'test' } });

      // update don't specify mergeObjects behavior
      updated = collection.updateByExample({ value: { foo: 'test' } }, { value: { } });
      assertEqual(1, updated);

      var doc = collection.document('one');
      assertEqual({ foo: 'test' }, doc.value);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: query-by-example
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryByExampleEdgeSuite () {
  'use strict';
  var cn = 'UnitTestsCollectionByExample';
  var c1 = 'UnitTestsCollectionByExampleEdge';
  var collection = null;
  var edge = null;

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      db._drop(c1);
      collection = db._create(cn);
      edge = db._createEdgeCollection(c1);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      collection.drop();
      edge.drop();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using edge index
// //////////////////////////////////////////////////////////////////////////////

    testByExampleEdgeFrom: function () {
      edge.save(cn + '/foo', cn + '/bar', { _key: 'e1', foo: 'bar' });
      edge.save(cn + '/bar', cn + '/foo', { _key: 'e2', bar: 'foo' });

      var doc;
      doc = edge.firstExample({ _from: cn + '/foo' });
      assertEqual(c1 + '/e1', doc._id);
      assertEqual('e1', doc._key);
      assertEqual('bar', doc.foo);
      assertEqual(cn + '/foo', doc._from);
      assertEqual(cn + '/bar', doc._to);

      doc = edge.firstExample({ _from: cn + '/barbaz' });
      assertNull(doc);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using edge index
// //////////////////////////////////////////////////////////////////////////////

    testByExampleEdgeFromMore: function () {
      edge.save(cn + '/foo', cn + '/bar', { _key: 'e1', foo: 'bar' });
      edge.save(cn + '/bar', cn + '/foo', { _key: 'e2', bar: 'foo' });

      var doc;
      doc = edge.firstExample({ _from: cn + '/foo', foo: 'bar' });
      assertEqual(c1 + '/e1', doc._id);
      assertEqual('e1', doc._key);
      assertEqual('bar', doc.foo);
      assertEqual(cn + '/foo', doc._from);
      assertEqual(cn + '/bar', doc._to);

      doc = edge.firstExample({ _from: cn + '/foo', foo: 'baz' });
      assertNull(doc);

      doc = edge.firstExample({ _from: cn + '/foo', bar: 'foo' });
      assertNull(doc);

      doc = edge.firstExample({ _from: cn + '/foo', boo: 'far' });
      assertNull(doc);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using edge index
// //////////////////////////////////////////////////////////////////////////////

    testByExampleEdgeTo: function () {
      edge.save(cn + '/foo', cn + '/bar', { _key: 'e1', foo: 'bar' });
      edge.save(cn + '/bar', cn + '/foo', { _key: 'e2', bar: 'foo' });

      var doc;
      doc = edge.firstExample({ _to: cn + '/foo' });
      assertEqual(c1 + '/e2', doc._id);
      assertEqual('e2', doc._key);
      assertEqual('foo', doc.bar);
      assertEqual(cn + '/bar', doc._from);
      assertEqual(cn + '/foo', doc._to);

      doc = edge.firstExample({ _to: cn + '/bart' });
      assertNull(doc);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using edge index
// //////////////////////////////////////////////////////////////////////////////

    testByExampleEdgeToMore: function () {
      edge.save(cn + '/foo', cn + '/bar', { _key: 'e1', foo: 'bar' });
      edge.save(cn + '/bar', cn + '/foo', { _key: 'e2', bar: 'foo' });

      var doc;
      doc = edge.firstExample({ _to: cn + '/foo', bar: 'foo' });
      assertEqual(c1 + '/e2', doc._id);
      assertEqual('e2', doc._key);
      assertEqual('foo', doc.bar);
      assertEqual(cn + '/bar', doc._from);
      assertEqual(cn + '/foo', doc._to);

      doc = edge.firstExample({ _to: cn + '/foo', bar: 'baz' });
      assertNull(doc);

      doc = edge.firstExample({ _to: cn + '/foo', foo: 'bar' });
      assertNull(doc);

      doc = edge.firstExample({ _to: cn + '/foo', boo: 'far' });
      assertNull(doc);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using edge index
// //////////////////////////////////////////////////////////////////////////////

    testByExampleEdgeFromTo: function () {
      edge.save(cn + '/foo', cn + '/bar', { _key: 'e1', foo: 'bar' });
      edge.save(cn + '/bar', cn + '/foo', { _key: 'e2', bar: 'foo' });

      var doc;
      doc = edge.firstExample({ _to: cn + '/foo', _from: cn + '/bar' });
      assertEqual(c1 + '/e2', doc._id);
      assertEqual('e2', doc._key);
      assertEqual('foo', doc.bar);
      assertEqual(cn + '/bar', doc._from);
      assertEqual(cn + '/foo', doc._to);

      doc = edge.firstExample({ _to: cn + '/foo', _from: cn + '/baz' });
      assertNull(doc);

      doc = edge.firstExample({ _to: cn + '/bar', _from: cn + '/bar' });
      assertNull(doc);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: byExample, using edge index
// //////////////////////////////////////////////////////////////////////////////

    testByExampleEdgeFromToMore: function () {
      edge.save(cn + '/foo', cn + '/bar', { _key: 'e1', foo: 'bar' });
      edge.save(cn + '/bar', cn + '/foo', { _key: 'e2', bar: 'foo' });

      var doc;
      doc = edge.firstExample({ _to: cn + '/foo', _from: cn + '/bar', bar: 'foo' });
      assertEqual(c1 + '/e2', doc._id);
      assertEqual('e2', doc._key);
      assertEqual('foo', doc.bar);
      assertEqual(cn + '/bar', doc._from);
      assertEqual(cn + '/foo', doc._to);

      doc = edge.firstExample({ _to: cn + '/foo', _from: cn + '/baz', bar: 'baz' });
      assertNull(doc);

      doc = edge.firstExample({ _to: cn + '/foo', _from: cn + '/baz', foo: 'bar' });
      assertNull(doc);

      doc = edge.firstExample({ _to: cn + '/bar', _from: cn + '/bar', boo: 'far' });
      assertNull(doc);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: range
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryRangeSuite () {
  'use strict';
  var cn = 'UnitTestsCollectionRange';
  var collection = null;
  var age = function (d) { return d.age; };
  var ageSort = function (l, r) { if (l !== r) { if (l < r) { return -1; } return 1; } return 0; };

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      collection = db._create(cn);

      for (var i = 0; i < 100; ++i) {
        collection.save({ age: i });
      }

      collection.ensureSkiplist('age');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      collection.drop();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: range
// //////////////////////////////////////////////////////////////////////////////

    testRange: function () {
      var l = collection.range('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12 ], l);

      l = collection.closedRange('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12, 13 ], l);

      l = collection.range('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ], l);

      l = collection.closedRange('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ], l);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: range
// //////////////////////////////////////////////////////////////////////////////

function SimpleQuerySparseRangeSuite () {
  'use strict';
  var cn = 'UnitTestsCollectionRange';
  var collection = null;
  var age = function (d) { return d.age; };
  var ageSort = function (l, r) { if (l !== r) { if (l < r) { return -1; } return 1; } return 0; };

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      collection = db._create(cn);

      for (var i = 0; i < 100; ++i) {
        collection.save({ age: i });
      }

      collection.ensureSkiplist('age', { sparse: true });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      collection.drop();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: range
// //////////////////////////////////////////////////////////////////////////////

    testSparseRange: function () {
      var l = collection.range('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12 ], l);

      l = collection.closedRange('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12, 13 ], l);

      l = collection.range('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ], l);

      l = collection.closedRange('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ], l);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: range
// //////////////////////////////////////////////////////////////////////////////

    testSparseRangeMultipleIndexes: function () {
      // now we have a sparse and a non-sparse index
      collection.ensureSkiplist('age', { sparse: false });

      var l = collection.range('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12 ], l);

      l = collection.closedRange('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12, 13 ], l);

      l = collection.range('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ], l);

      l = collection.closedRange('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ], l);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: range
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryUniqueRangeSuite () {
  'use strict';
  var cn = 'UnitTestsCollectionRange';
  var collection = null;
  var age = function (d) { return d.age; };
  var ageSort = function (l, r) { if (l !== r) { if (l < r) { return -1; } return 1; } return 0; };

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      collection = db._create(cn);

      for (var i = 0; i < 100; ++i) {
        collection.save({ age: i });
      }

      collection.ensureUniqueSkiplist('age');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      collection.drop();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: range
// //////////////////////////////////////////////////////////////////////////////

    testUniqueRange: function () {
      var l = collection.range('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12 ], l);

      l = collection.closedRange('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12, 13 ], l);

      l = collection.range('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ], l);

      l = collection.closedRange('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ], l);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: range
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryUniqueSparseRangeSuite () {
  'use strict';
  var cn = 'UnitTestsCollectionRange';
  var collection = null;
  var age = function (d) { return d.age; };
  var ageSort = function (l, r) { if (l !== r) { if (l < r) { return -1; } return 1; } return 0; };

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(cn);
      collection = db._create(cn);

      for (var i = 0; i < 100; ++i) {
        collection.save({ age: i });
      }

      collection.ensureUniqueSkiplist('age', { sparse: true });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      collection.drop();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: range
// //////////////////////////////////////////////////////////////////////////////

    testUniqueSparseRange: function () {
      var l = collection.range('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12 ], l);

      l = collection.closedRange('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12, 13 ], l);

      l = collection.range('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ], l);

      l = collection.closedRange('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ], l);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: range
// //////////////////////////////////////////////////////////////////////////////

    testUniqueSparseRangeMultipleIndexes: function () {
      // now we have a sparse and a non-sparse index
      collection.ensureUniqueSkiplist('age', { sparse: false });

      var l = collection.range('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12 ], l);

      l = collection.closedRange('age', 10, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 10, 11, 12, 13 ], l);

      l = collection.range('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ], l);

      l = collection.closedRange('age', null, 13).toArray().map(age).sort(ageSort);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ], l);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite: range
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryAnySuite () {
  'use strict';
  var cn = 'UnitTestsCollectionAny';
  var collectionEmpty = null;
  var collectionOne = null;

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      var name;

      name = cn + 'Empty';
      db._drop(name);
      collectionEmpty = db._create(name);

      name = cn + 'One';
      db._drop(name);
      collectionOne = db._create(name, { waitForSync: false });
      collectionOne.save({ age: 1 });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      collectionEmpty.drop();
      collectionOne.drop();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test: any
// //////////////////////////////////////////////////////////////////////////////

    testAny: function () {
      var d = collectionEmpty.any();
      assertEqual(null, d);

      d = collectionOne.any();
      assertNotNull(d);
      assertEqual(1, d.age);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suites
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(SimpleQueryLookupByKeysSuite);
jsunity.run(SimpleQueryRemoveByKeysSuite);
jsunity.run(SimpleQueryArraySkipLimitSuite);
jsunity.run(SimpleQueryAllSkipLimitSuite);
jsunity.run(SimpleQueryByExampleSuite);
jsunity.run(SimpleQueryByExampleEdgeSuite);
jsunity.run(SimpleQueryRangeSuite);
jsunity.run(SimpleQuerySparseRangeSuite);
jsunity.run(SimpleQueryUniqueRangeSuite);
jsunity.run(SimpleQueryUniqueSparseRangeSuite);
jsunity.run(SimpleQueryAnySuite);

return jsunity.done();


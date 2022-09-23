/* jshint esnext: true */
/* global AQL_EXECUTE, AQL_EXPLAIN, AQL_EXECUTEJSON */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for the AQL FOR x IN GRAPH name statement
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, fail} = jsunity.jsUnity.assertions;

const internal = require('internal');
const db = internal.db;


const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';










function limitSuite() {
  const gn = 'UnitTestGraph';

  return {

    setUpAll: function () {
      db._drop(gn + 'v');
      db._drop(gn + 'e');
      db._drop(gn + 'e2');

      var i;

      var c = db._create(gn + 'v');
      for (i = 0; i < 10000; ++i) {
        c.insert({_key: 'test' + i});
      }

      c = db._createEdgeCollection(gn + 'e');
      for (i = 0; i < 10000; ++i) {
        c.insert({_from: gn + 'v/test' + i, _to: gn + 'v/test' + i});
      }

      c = db._createEdgeCollection(gn + 'e2');
      c.insert({_from: gn + 'v/test1', _to: gn + 'v/test0'});
      c.insert({_from: gn + 'v/test2', _to: gn + 'v/test0'});
      c.insert({_from: gn + 'v/test2', _to: gn + 'v/test1'});
    },

    tearDownAll: function () {
      db._drop(gn + 'v');
      db._drop(gn + 'e');
      db._drop(gn + 'e2');
    },

    testLimits: function () {
      const queries = [
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 0, 10000 RETURN e', 10000],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 0, 1000 RETURN e', 1000],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 0, 100 RETURN e', 100],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 0, 10 RETURN e', 10],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 10, 10 RETURN e', 10],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 10, 100 RETURN e', 100],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 10, 1000 RETURN e', 1000],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 10, 10000 RETURN e', 9990],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 1000, 1 RETURN e', 1],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 1000, 10 RETURN e', 10],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 1000, 100 RETURN e', 100],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 1000, 1000 RETURN e', 1000],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 1000, 10000 RETURN e', 9000],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9990, 1 RETURN e', 1],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9990, 9 RETURN e', 9],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9990, 10 RETURN e', 10],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9990, 11 RETURN e', 10],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9990, 100 RETURN e', 10],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9990, 1000 RETURN e', 10],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9999, 1 RETURN e', 1],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9999, 10 RETURN e', 1],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9999, 100 RETURN e', 1],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 9999, 1000 RETURN e', 1],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 10000, 0 RETURN e', 0],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 10000, 1 RETURN e', 0],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 10000, 10 RETURN e', 0],
        ['FOR v IN ' + gn + 'v FOR e IN 1..1 OUTBOUND v._id ' + gn + 'e LIMIT 10000, 1000 RETURN e', 0]
      ];

      queries.forEach(function (query) {
        assertEqual(query[1], AQL_EXECUTE(query[0]).json.length, query);
      });
    },

    testLimitsMultiEdges: function () {
      var queries = [
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test0"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 RETURN e', 0],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test0"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 0, 1 RETURN e', 0],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test0"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 1, 1 RETURN e', 0],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test1"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 RETURN e', 1],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test1"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 0, 1 RETURN e', 1],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test1"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 1, 1 RETURN e', 0],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test1"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 2, 1 RETURN e', 0],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test2"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 RETURN e', 2],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test2"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 0, 1 RETURN e', 1],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test2"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 0, 1 RETURN e', 1],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test2"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 0, 2 RETURN e', 2],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test2"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 1, 1 RETURN e', 1],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test2"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 1, 2 RETURN e', 1],
        ['WITH ' + gn + 'v FOR v IN ["' + gn + 'v/test2"] FOR e IN 1..1 OUTBOUND v ' + gn + 'e2 LIMIT 2, 1 RETURN e', 0]
      ];

      queries.forEach(function (query) {
        assertEqual(query[1], AQL_EXECUTE(query[0]).json.length, query);
      });
    }

  };
}

jsunity.run(limitSuite);
return jsunity.done();

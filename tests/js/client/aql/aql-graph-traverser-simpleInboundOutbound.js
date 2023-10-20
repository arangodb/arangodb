/* jshint esnext: true */

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
const db = require('internal').db;


const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';


function simpleInboundOutboundSuite() {
  const gn = 'UnitTestGraph';

  return {

    setUp: function () {
      db._drop(gn + 'v1');
      db._drop(gn + 'v2');
      db._drop(gn + 'e');

      let c;
      c = db._create(gn + 'v1', {numberOfShards: 9});
      c.insert({_key: "test"});

      c = db._create(gn + 'v2', {numberOfShards: 7});
      c.insert({_key: "test"});

      c = db._createEdgeCollection(gn + 'e', {numberOfShards: 5});
      c.insert({_from: gn + "v2/test", _to: gn + "v1/test"});
    },

    tearDown: function () {
      db._drop(gn + 'v1');
      db._drop(gn + 'v2');
      db._drop(gn + 'e');
    },

    testTheOldInAndOutOut: function () {
      // outbound
      let q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN OUTBOUND DOCUMENT("${gn + 'v2'}/test") ${gn + 'e'} RETURN {v, e}`;
      let res = db._query(q).toArray()[0];

      assertEqual(gn + "v1/test", res.v._id);
      assertEqual("test", res.v._key);
      assertEqual(gn + "v2/test", res.e._from);
      assertEqual(gn + "v1/test", res.e._to);

      // same test, but now reverse
      q = `WITH ${gn + 'v1'} ${gn + 'v2'} FOR v, e IN INBOUND DOCUMENT("${gn + 'v1'}/test") ${gn + 'e'} RETURN {v, e}`;
      res = db._query(q).toArray()[0];

      assertEqual(gn + "v2/test", res.v._id);
      assertEqual("test", res.v._key);
      assertEqual(gn + "v2/test", res.e._from);
      assertEqual(gn + "v1/test", res.e._to);
    }

  };
}

jsunity.run(simpleInboundOutboundSuite);
return jsunity.done();

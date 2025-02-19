/* jshint esnext: true */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
const errors = require('@arangodb').errors;

const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';



const gh = require('@arangodb/graph/helpers');

function potentialErrorsSuite() {
  var vc, ec;
  var vertex = {};
  

  return {

    setUpAll: function () {
      gh.cleanup();
      vc = db._create(vn);
      ec = db._createEdgeCollection(en);
      vertex.A = vn + '/unknown';

      vertex.B = vc.save({_key: 'B'})._id;
      vertex.C = vc.save({_key: 'C'})._id;
      ec.save(vertex.B, vertex.C, {});
    },

    tearDownAll: gh.cleanup,

    testNonIntegerSteps: function () {
      var query = 'FOR x IN 2.5 OUTBOUND @startId @@eCol RETURN x';
      var bindVars = {
        '@eCol': en,
        'startId': vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testNonNumberSteps: function () {
      var query = 'FOR x IN "invalid" OUTBOUND @startId @@eCol RETURN x';
      var bindVars = {
        '@eCol': en,
        'startId': vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testMultiDirections: function () {
      var query = 'FOR x IN OUTBOUND ANY @startId @@eCol RETURN x';
      var bindVars = {
        '@eCol': en,
        'startId': vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testNoCollections: function () {
      var query = 'FOR x IN OUTBOUND @startId RETURN x';
      var bindVars = {
        'startId': vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testNoStartVertex: function () {
      var query = 'FOR x IN OUTBOUND @@eCol RETURN x';
      var bindVars = {
        '@eCol': en
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testTooManyOutputParameters: function () {
      var query = 'FOR x, y, z, f IN OUTBOUND @startId @@eCol RETURN x';
      var bindVars = {
        '@eCol': en,
        'startId': vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testTraverseVertexCollection: function () {
      var query = 'FOR x IN OUTBOUND @startId @@eCol, @@vCol RETURN x';
      var bindVars = {
        '@eCol': en,
        '@vCol': vn,
        'startId': vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
        fail(query + ' should not be allowed');
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code);
      }
    },

    testStartWithSubquery: function () {
      var query = 'FOR x IN OUTBOUND (FOR y IN @@vCol SORT y._id LIMIT 3 RETURN y) @@eCol SORT x._id RETURN x';
      var bindVars = {
        '@eCol': en,
        '@vCol': vn
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testStepsSubquery: function () {
      var query = `WITH ${vn}
      FOR x IN (FOR y IN 1..1 RETURN y) OUTBOUND @startId @@eCol
      RETURN x`;
      var bindVars = {
        '@eCol': en,
        'startId': vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testCrazyStart1: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND null @@eCol
      RETURN x`;
      var bindVars = {
        '@eCol': en
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testCrazyStart2: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND 1 @@eCol
      RETURN x`;
      var bindVars = {
        '@eCol': en
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testCrazyStart3: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND [] @@eCol
      RETURN x`;
      var bindVars = {
        '@eCol': en
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testCrazyStart4: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND "foobar" @@eCol
      RETURN x`;
      var bindVars = {
        '@eCol': en
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testCrazyStart5: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND {foo: "bar"} @@eCol
      RETURN x`;
      var bindVars = {
        '@eCol': en
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testCrazyStart6: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND {_id: @startId} @@eCol
      RETURN x._id`;
      var bindVars = {
        'startId': vertex.B,
        '@eCol': en
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testCrazyStart7: function () {
      var query = `FOR x IN OUTBOUND
      (FOR y IN @@vCol FILTER y._id == @startId RETURN y) @@eCol
      RETURN x._id`;
      var bindVars = {
        'startId': vertex.B,
        '@eCol': en,
        '@vCol': vn
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
      // Fix the query, just use the first value
      query = `WITH ${vn}
      FOR x IN OUTBOUND
      (FOR y IN @@vCol FILTER y._id == @startId RETURN y)[0] @@eCol
      RETURN x._id`;
      result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testCrazyStart8: function () {
      var query = `WITH ${vn}
      FOR x IN OUTBOUND
      (FOR y IN @@eCol FILTER y._id == @startId RETURN "peter") @@eCol
      RETURN x._id`;
      var bindVars = {
        'startId': vertex.A,
        '@eCol': en
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
      // Actually use the string!
      query = `WITH ${vn}
      FOR x IN OUTBOUND
      (FOR y IN @@eCol FILTER y._id == @startId RETURN "peter")[0] @@eCol
      RETURN x._id`;
      result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 0);
    }

  };
}

jsunity.run(potentialErrorsSuite);
return jsunity.done();

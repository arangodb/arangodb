/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server parameters
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'query.allow-collections-in-expressions': "false",
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
const db = require('internal').db;

function testSuite() {
  return {
    setUpAll: function() {
      db._drop(cn);
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 500; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      c.insert(docs);
    },
    
    tearDownAll: function() {
      db._drop(cn);
    },

    testUseInForLoop: function() {
      let result = db._query("FOR doc IN " + cn + " RETURN doc").toArray();
      assertEqual(500, result.length);
      result.forEach((doc) => {
        assertTrue(doc.hasOwnProperty('_key'));
        assertTrue(doc.hasOwnProperty('value'));
      });
    },

    testUseInSubquery: function() {
      let result = db._query("LET sub = (FOR doc IN " + cn + " RETURN doc) RETURN sub").toArray();
      assertEqual(1, result.length);
      assertEqual(500, result[0].length);
      result[0].forEach((doc) => {
        assertTrue(doc.hasOwnProperty('_key'));
        assertTrue(doc.hasOwnProperty('value'));
      });
    },

    testUseInExpression1: function() {
      try {
        db._query("RETURN " + cn).toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION.code, err.errorNum);
      }
    },
    
    testUseInExpression2: function() {
      try {
        db._query("RETURN " + cn + "[*].value").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION.code, err.errorNum);
      }
    },
    
    testUseInExpression3: function() {
      try {
        db._query("FOR value IN " + cn + "[*].value RETURN value").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION.code, err.errorNum);
      }
    },
    
    testUseInExpression4: function() {
      try {
        db._query("FOR doc IN " + cn + " RETURN " + cn + "[0]").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION.code, err.errorNum);
      }
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();

/*jshint globalstrict:false, strict:false, maxlen:1000*/
/*global assertEqual, assertTrue, assertFalse, fail, more */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const functions = require('@arangodb/aql/functions');
const db = arangodb.db;

function V8QuerySuite () {
  'use strict';
  return {
    setUpAll : function () {
      try {
        functions.unregister('test::testudf');
      } catch (err) {}
      
      functions.register('test::testudf', function() { return 42; });
    },
    
    tearDownAll : function () {
      try {
        functions.unregister('test::testudf');
      } catch (err) {}
    },

    testCallV8 : function () {
      let results = db._query({ query: "RETURN CALL('V8', 'abcde')" }).toArray();
      assertEqual('abcde', results[0]);
    },
    
    testCallV8Hidden : function () {
      let results = db._query({ query: "RETURN CALL(NOOPT('V8'), 'abcde')" }).toArray();
      assertEqual('abcde', results[0]);
    },
    
    testApplyV8 : function () {
      let results = db._query({ query: "RETURN APPLY('V8', [ 'abcde' ])" }).toArray();
      assertEqual('abcde', results[0]);
    },
    
    testApplyV8Hidden : function () {
      let results = db._query({ query: "RETURN APPLY(NOOPT('V8'), [ 'abcde' ])" }).toArray();
      assertEqual('abcde', results[0]);
    },

    testCallNonV8 : function () {
      let results = db._query({ query: "RETURN CALL('SUBSTRING', 'abcde', 1, 2)" }).toArray();
      assertEqual('bc', results[0]);
    },
    
    testCallNonV8Hidden : function () {
      let results = db._query({ query: "RETURN CALL(NOOPT('SUBSTRING'), 'abcde', 1, 2)" }).toArray();
      assertEqual('bc', results[0]);
    },
    
    testApplyNonV8 : function () {
      let results = db._query({ query: "RETURN APPLY('SUBSTRING', ['abcde', 1, 2])" }).toArray();
      assertEqual('bc', results[0]);
    },
    
    testApplyNonV8Hidden : function () {
      let results = db._query({ query: "RETURN APPLY(NOOPT('SUBSTRING'), ['abcde', 1, 2])" }).toArray();
      assertEqual('bc', results[0]);
    },
    
    testCallUserDefined : function () {
      let results = db._query({ query: "RETURN CALL('test::testudf')" }).toArray();
      assertEqual(42, results[0]);
    },
    
    testCallUserDefinedHidden : function () {
      let results = db._query({ query: "RETURN CALL(NOOPT('test::testudf'))" }).toArray();
      assertEqual(42, results[0]);
    },
    
    testApplyUserDefined : function () {
      let results = db._query({ query: "RETURN APPLY('test::testudf', [])" }).toArray();
      assertEqual(42, results[0]);
    },
    
    testApplyUserDefinedHidden : function () {
      let results = db._query({ query: "RETURN APPLY(NOOPT('test::testudf'), [])" }).toArray();
      assertEqual(42, results[0]);
    },

  };
}

jsunity.run(V8QuerySuite);
return jsunity.done();

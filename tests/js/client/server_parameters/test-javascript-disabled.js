/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'javascript.enabled': "false"
  };
}
let jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
let db = require('internal').db;

function testSuite() {
  return {
    testCallNonExistingJavaScriptRestRoute : function() {
      let result = arango.GET('/dudel/doedel');
      assertEqual(errors.ERROR_NOT_IMPLEMENTED.code, result.errorNum);
    },
    
    testCallReloadRouting : function() {
      try {
        require('internal').reloadRouting();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_NOT_IMPLEMENTED.code, err.errorNum);
      }
    },
    
    testRunSimpleQuery : function() {
      try {
        db['_graphs'].any();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_NOT_IMPLEMENTED.code, err.errorNum);
      }
    },
    
    testRegisterAqlUDF : function() {
      try {
        require('@arangodb/aql/functions').register('foo::bar', function () { return 1; });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_NOT_IMPLEMENTED.code, err.errorNum);
      }
    },
    
    testRunJavaScriptTransaction : function() {
      try {
        db._executeTransaction({
          collections: {},
          action: function() { return 1; }
        });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_NOT_IMPLEMENTED.code, err.errorNum);
      }
    },
    
    testRegisterJavaScriptTask : function() {
      try {
        require('@arangodb/tasks').register({
          name: 'UnitTestTasks',
          command: 'return 1;'
        });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_NOT_IMPLEMENTED.code, err.errorNum);
      }
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();

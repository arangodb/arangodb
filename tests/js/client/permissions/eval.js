/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertUndefined, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for security settings
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
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const fs = require("fs");
const testFile = fs.join(fs.getTempPath(), "test.js");

if (getOptions === true) {
  fs.write(testFile, `print('hello world')`);
  return {
    'temp.path': fs.getTempPath(),     // Adjust the temp-path to match our current temp path
    'javascript.files-black-list': [
      testFile
    ]
  };
}

var jsunity = require('jsunity');
const arangodb = require("@arangodb");

function testSuite() {
  return {
    setUp: function() {},
    tearDown: function() {},
    testEval: function() {
      try {
        require("internal").parseFile(testFile);
        fail();
      } catch (err) {
        assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum);
      }
      // we can not forbid snippet evaluation.
      // Access is file access based for now.
      require("internal").parse(`print('hello world')`);
      try {
        require("internal").load(testFile);
        fail();
      } catch (err) {
        assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum);
      }
    },
  };
}
jsunity.run(testSuite);
return jsunity.done();

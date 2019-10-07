/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertNotMatch, assertUndefined */

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

if (getOptions === true) {
  return {
    'javascript.startup-options-blacklist': 'point|log'
  };
}

var jsunity = require('jsunity');
var env = require('process').env;
function testSuite() {
  return {
    setUp: function() {},
    tearDown: function() {},
    
    testKeyPattern : function() {
      const regex = new RegExp('.*point.*|log');

      let opts = require('internal').options();
      Object.keys(opts).forEach(function(key) {
        assertNotMatch(regex, key);
      });
    },
    
    testIncluded : function() {
      let opts = require('internal').options();
      assertTrue(opts.hasOwnProperty('temp.path'));
      assertTrue(typeof opts['temp.path'] === 'string');
    },

    testDedicatedOptions : function() {
      let opts = require('internal').options();

      assertUndefined(opts['javascript.endpoints-filter']);
      assertFalse(opts.hasOwnProperty('javascript.endpoints-filter'));

      assertUndefined(opts['server.endpoint']);
      assertFalse(opts.hasOwnProperty('server.endpoint'));

      assertUndefined(opts['log.file']);
      assertFalse(opts.hasOwnProperty('log.file'));

      assertUndefined(opts['log.output']);
      assertFalse(opts.hasOwnProperty('log.output'));

      assertTrue(opts.hasOwnProperty('console.pager'));
      assertFalse(opts['console.pager']);

      assertTrue(opts.hasOwnProperty('console.pager-command'));
      assertTrue(opts['console.pager-command'].length > 0);
    }
  };
}
jsunity.run(testSuite);
return jsunity.done();

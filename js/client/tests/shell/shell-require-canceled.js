/*jshint globalstrict:false, strict:false */
/*global assertEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the require which is canceled
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for 'require-canceled'
////////////////////////////////////////////////////////////////////////////////

function RequireCanceledTestSuite() {
  'use strict';

  return {
    setUp() {
        arango.POST_RAW("/_admin/execute",
          "require('module').globalPaths.unshift(require('path').resolve('./js/common/test-data/modules'));", {
            'x-arango-v8-context': 0
          });
      },

      tearDown() {
        arango.POST_RAW("/_admin/execute",
          "require('module').globalPaths.splice(0,1);", {
            'x-arango-v8-context': 0
          });
      },

      testRequireJson() {
        var internal = require("internal");
        var a = arango.POST_RAW("/_admin/execute",
          'return Object.keys(require("a"));', {
            'x-arango-async': "store",
            'x-arango-v8-context': 0
          });

        internal.sleep(3);

        var id = a.headers['x-arango-async-id'];
        arango.PUT_RAW("/_api/job/" + id + "/cancel", '');

        var c = arango.POST_RAW("/_admin/execute?returnAsJSON=true",
          'return Object.keys(require("a"));', {
            'x-arango-async': "false",
            'x-arango-v8-context': 0
          });

        var d;

        try {
          d = JSON.parse(c.body);
        } catch (err) {
          require("internal").print(c.body);
          throw err;
        }

        assertEqual(2, d.length);
      }
  };
}


jsunity.run(RequireCanceledTestSuite);

return jsunity.done();

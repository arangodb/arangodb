/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, assertFalse, arango */

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
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'log.api-enabled': "true"
  };
}

const jsunity = require('jsunity');

function testSuite() {
  return {
    testApiGetEntries : function() {
      let res = arango.GET("/_admin/log/entries");
      assertTrue(res.hasOwnProperty("total"));
      assertTrue(res.hasOwnProperty("messages"));
      assertTrue(Array.isArray(res.messages));
      res.messages.forEach((message) => {
        assertTrue(message.hasOwnProperty("id"));
        assertTrue(message.hasOwnProperty("topic"));
        assertTrue(message.hasOwnProperty("level"));
        assertTrue(message.hasOwnProperty("date"));
        assertTrue(message.hasOwnProperty("message"));
      });
    },
    
    testApiGetLevel : function() {
      const levels = ["FATAL", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE", "DEFAULT"];

      let res = arango.GET("/_admin/log/level");
      Object.keys(res).forEach((k) => {
        let level = res[k];
        assertNotEqual(-1, levels.indexOf(level), level);
      });
    },
    
    testApiPutLevel : function() {
      const levels = ["FATAL", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE", "DEFAULT"];

      let res = arango.PUT("/_admin/log/level", {});
      Object.keys(res).forEach((k) => {
        let level = res[k];
        assertNotEqual(-1, levels.indexOf(level));
      });

      levels.forEach((level) => {
        res = arango.PUT("/_admin/log/level", { "replication": level });
        Object.keys(res).forEach((k) => {
          let level = res[k];
          assertNotEqual(-1, levels.indexOf(level));
        });

        assertEqual(level, res["replication"]);
      });
    },
    
    testApiDeleteEntries : function() {
      let res = arango.DELETE("/_admin/log/entries");
      assertEqual(200, res.code);
    },

    testDeprecatedApiGet : function() {
      let res = arango.GET("/_admin/log");
      assertTrue(res.error);
      assertEqual(410, res.code);
    },

    testDeprecatedApiDelete : function() {
      let res = arango.DELETE("/_admin/log");
      assertTrue(res.error);
      assertEqual(410, res.code);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();

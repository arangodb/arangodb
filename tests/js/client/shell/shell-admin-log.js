/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertMatch, assertTrue, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');

function adminLogSuite() {
  'use strict';
      
  let log = function(level) {
    arango.POST("/_admin/execute", `for (let i = 0; i < 50; ++i) require('console')._log('general=${level}', 'testi');`);
  };

  return {
    setUp : function() {
      arango.DELETE("/_admin/log");
    },

    testGetAdminLogOldFormat: function () {
      log("warn");
      let res = arango.GET("/_admin/log?upto=warning");
      let level = 2;
      assertEqual(50, res.totalAmount, res);
      assertEqual(50, res.lid.length, res);
      assertEqual(50, res.topic.length, res);
      assertEqual(50, res.level.length, res);
      res.level.forEach((l) => assertEqual(level, l, res));
      assertEqual(50, res.timestamp.length, res);
      assertEqual(50, res.text.length, res);
      res.text.forEach((t) => assertMatch(/testi/, t, res));
    },
    
    testGetAdminLogNewFormat: function () {
      log("warn");
      let res = arango.GET("/_admin/log/entries?upto=warning");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);

      let lastId = null;
      res.messages.forEach((entry) => {
        assertEqual("number", typeof entry.id);
        assertTrue(entry.id > lastId || lastId === null);
        lastId = entry.id;
        assertEqual("WARNING", entry.level);
        assertEqual("general", entry.topic);
        assertMatch(/^\d+-\d+-\d+T\d+:\d+:\d+/, entry.date);
        assertMatch(/testi/, entry.message);
      });
    },
    
    testGetAdminLogNewFormatUpToFatal: function () {
      log("fatal");
      
      let res = arango.GET("/_admin/log/entries?upto=fatal");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=err");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);

      res = arango.GET("/_admin/log/entries?upto=info");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
    },
    
    testGetAdminLogNewFormatUpToErr: function () {
      log("error");
      
      let res = arango.GET("/_admin/log/entries?upto=fatal");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(0, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=error");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);

      res = arango.GET("/_admin/log/entries?upto=info");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
    },
    
    testGetAdminLogNewFormatUpToWarn: function () {
      log("warn");
      
      let res = arango.GET("/_admin/log/entries?upto=fatal");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(0, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=err");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(0, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);

      res = arango.GET("/_admin/log/entries?upto=info");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
    },
    
    testGetAdminLogNewFormatUpToInfo: function () {
      log("info");
      
      let res = arango.GET("/_admin/log/entries?upto=fatal");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(0, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=err");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(0, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(0, res.total);

      res = arango.GET("/_admin/log/entries?upto=info");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
    },
    
    testGetAdminLogNewFormatReverse: function () {
      log("warn");
      let res = arango.GET("/_admin/log/entries?upto=warning&sort=desc");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);

      let lastId = null;
      res.messages.forEach((entry) => {
        assertEqual("number", typeof entry.id);
        assertTrue(entry.id < lastId || lastId === null);
        lastId = entry.id;
        assertEqual("WARNING", entry.level);
        assertEqual("general", entry.topic);
        assertMatch(/^\d+-\d+-\d+T\d+:\d+:\d+/, entry.date);
        assertMatch(/testi/, entry.message);
      });
    },
    
    testGetAdminLogNewFormatSearch: function () {
      log("warn");
      let res = arango.GET("/_admin/log/entries?upto=warning&search=testi");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);

      res.messages.forEach((entry) => {
        assertMatch(/testi/, entry.message);
      });
      
      res = arango.GET("/_admin/log/entries?upto=warning&search=testi&size=5");
      assertTrue(Array.isArray(res.messages));
      assertEqual(5, res.messages.length);
      assertEqual(50, res.total);

      res.messages.forEach((entry) => {
        assertMatch(/testi/, entry.message);
      });
      
      res = arango.GET("/_admin/log/entries?upto=warning&search=fuxxmann");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.total);
      assertEqual(0, res.messages.length);
    },
    
    testGetAdminLogNewFormatSize: function () {
      log("warn");
      let res = arango.GET("/_admin/log/entries?upto=warning&size=4");
      assertTrue(Array.isArray(res.messages));
      assertEqual(4, res.messages.length);
      assertEqual(50, res.total);

      res = arango.GET("/_admin/log/entries?upto=warning&size=10");
      assertTrue(Array.isArray(res.messages));
      assertEqual(10, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&size=50");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&size=52");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&size=100000");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
    },
   
    testGetAdminLogNewFormatOffset: function () {
      log("warn");
      let res = arango.GET("/_admin/log/entries?upto=warning&offset=0&size=4");
      assertTrue(Array.isArray(res.messages));
      assertEqual(4, res.messages.length);
      assertEqual(50, res.total);

      res = arango.GET("/_admin/log/entries?upto=warning&offset=0&size=10");
      assertTrue(Array.isArray(res.messages));
      assertEqual(10, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=0&size=49");
      assertTrue(Array.isArray(res.messages));
      assertEqual(49, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=0&size=50");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=0&size=51");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=0&size=100000");
      assertTrue(Array.isArray(res.messages));
      assertEqual(50, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=48&size=0");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=48&size=1");
      assertTrue(Array.isArray(res.messages));
      assertEqual(1, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=48&size=2");
      assertTrue(Array.isArray(res.messages));
      assertEqual(2, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=48&size=3");
      assertTrue(Array.isArray(res.messages));
      assertEqual(2, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=49&size=0");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=49&size=1");
      assertTrue(Array.isArray(res.messages));
      assertEqual(1, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=49&size=2");
      assertTrue(Array.isArray(res.messages));
      assertEqual(1, res.messages.length);
      assertEqual(50, res.total);

      res = arango.GET("/_admin/log/entries?upto=warning&offset=50&size=0");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=50&size=1");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(50, res.total);
      
      res = arango.GET("/_admin/log/entries?upto=warning&offset=50&size=100");
      assertTrue(Array.isArray(res.messages));
      assertEqual(0, res.messages.length);
      assertEqual(50, res.total);
    },
    
  };
}

jsunity.run(adminLogSuite);
return jsunity.done();

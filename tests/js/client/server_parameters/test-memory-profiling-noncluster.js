/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertMatch, assertNotMatch, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for memory profiling
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
/// @author Max Neunhoeffer
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  require("internal").env["MALLOC_CONF"] = "prof:true";
  return {
  };
}
const jsunity = require('jsunity');
const db = require('internal').db;

function testSuite() {
  return {
    testMemoryProfile: function() {
      let v = arango.GET("/_api/version?details=true");
      if (v.details.jemalloc != "true" || 
          v.details.libunwind != "true" || 
          v.details["memory-profiler"] != "true") {
        print("Skipping memory profiler test...");
        return;
      }
      let p = arango.GET_RAW("/_admin/status?memory=true");
      assertEqual(200, p.code);
      print(p);
      assertEqual("heap_v2/", p.body.slice(0, 8));
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();

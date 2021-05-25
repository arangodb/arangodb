/*jshint globalstrict:false, strict:false */
/* global arango, getOptions, runSetup, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
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

const db = require('@arangodb').db;
const cn = 'UnitTestsCollection';

if (getOptions === true) {
  return {
    'foxx.queues' : 'false',
    'server.statistics' : 'false',
    runSetup: true,
  };
}
if (runSetup === true) {
  let payload = Array(1024).join("x"); 
  let c = db._create(cn);
  let docs = [];
  for (let i = 0; i < 250 * 1000; ++i) {
    docs.push({ _key: "test" + i, value1: i, value2: "test" + i, payload });
    if (docs.length === 1000) {
      c.insert(docs);
      docs = [];
    }
  }
  return true;
}

const jsunity = require('jsunity');

function FillBlockCacheSuite() {
  'use strict';

  return {
    
    testHttpApi: function() {
      const oldValue = db._engineStats()["rocksdb.block-cache-usage"];
      arango.POST("/_api/cursor", { query: "FOR doc IN @@collection RETURN doc.value1", bindVars: { "@collection": cn }, options: { fillBlockCache: true } });
      
      const newValue = db._engineStats()["rocksdb.block-cache-usage"];
      assertTrue(newValue >= oldValue + 100 * 1000 * 1000, { oldValue, newValue });
      
      arango.POST("/_api/cursor", { query: "FOR doc IN @@collection RETURN doc.value1", bindVars: { "@collection": cn }, options: { fillBlockCache: true } });
      
      // allow for some other background things to be run, thus add 1MB of leeway
      const newValue2 = db._engineStats()["rocksdb.block-cache-usage"];
      assertTrue(newValue2 <= newValue + 1000 * 1000, { newValue, newValue2 });
    },

  };
}

jsunity.run(FillBlockCacheSuite);
return jsunity.done();

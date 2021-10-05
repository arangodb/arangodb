/*jshint globalstrict:false, strict:false */
/* global arango, assertTrue, assertEqual */

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
const internal = require('internal');
const jsunity = require('jsunity');

exports.setup = function(cn) {
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
};

exports.testDbQuery = function(cn, fillBlockCache) {
  const oldValue = db._engineStats()["rocksdb.block-cache-usage"];
  let result = db._query("FOR doc IN @@collection RETURN doc.value1", { "@collection": cn }, { fillBlockCache }).toArray();
  assertEqual(250 * 1000, result.length);
  
  const newValue = db._engineStats()["rocksdb.block-cache-usage"];
  if (fillBlockCache) {
    assertTrue(newValue >= oldValue + 100 * 1000 * 1000, { oldValue, newValue });
  } else {
    // allow for some other background things to be run, thus add 1MB of leeway
    assertTrue(newValue <= oldValue + 1000 * 1000, { oldValue, newValue });
  }

  /*
   * for some reason the block cache usage is not 100% deterministic after here.
   * it is probably due to some other unrelated queries running or some RocksDB
   * internals
  result = db._query("FOR doc IN @@collection RETURN doc.value1", { "@collection": cn }, { fillBlockCache }).toArray();
  assertEqual(250 * 1000, result.length);

  if (internal.platform.substr(0, 3) !== 'win') {
    const newValue2 = db._engineStats()["rocksdb.block-cache-usage"];
    // allow for some other background things to be run, thus add 1MB of leeway
    // On Windows, the block cache usage in newValue2 is a lot higher here.
    assertTrue(newValue2 <= newValue + 1000 * 1000, { newValue, newValue2 });
  }
  */
};

exports.testHttpApi = function(cn, fillBlockCache) {
  const oldValue = db._engineStats()["rocksdb.block-cache-usage"];
  arango.POST("/_api/cursor", { query: "FOR doc IN @@collection RETURN doc.value1", bindVars: { "@collection": cn }, options: { fillBlockCache } });
  
  const newValue = db._engineStats()["rocksdb.block-cache-usage"];
  if (fillBlockCache) {
    assertTrue(newValue >= oldValue + 100 * 1000 * 1000, { oldValue, newValue });
  } else {
    // allow for some other background things to be run, thus add 1MB of leeway
    assertTrue(newValue <= oldValue + 1000 * 1000, { oldValue, newValue });
  }
  
  /*
   * for some reason the block cache usage is not 100% deterministic after here.
   * it is probably due to some other unrelated queries running or some RocksDB
   * internals
  arango.POST("/_api/cursor", { query: "FOR doc IN @@collection RETURN doc.value1", bindVars: { "@collection": cn }, options: { fillBlockCache } });
  
  if (internal.platform.substr(0, 3) !== 'win') {
    // for unknown reasons the following does not work reliably on Windows.
    // On Windows, the block cache usage in newValue2 is a lot higher here.
    const newValue2 = db._engineStats()["rocksdb.block-cache-usage"];
    // allow for some other background things to be run, thus add 1MB of leeway
    assertTrue(newValue2 <= newValue + 1000 * 1000, { newValue, newValue2 });
  }
  */
};

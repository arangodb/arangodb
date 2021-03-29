/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for transaction idle timeouts
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
    'transaction.streaming-idle-timeout': "60.0"
  };
}

const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const db = require('internal').db;

function testSuite() {
  return {
    testFastEnough : function() {
      let opts = { collections: {} };
      let trx = db._createTransaction(opts);
      let result = trx.status();
      assertEqual("running", result.status);

      result = trx.abort();
      assertEqual("aborted", result.status);
    },
    
    testPushForward : function() {
      let opts = { collections: {} };
      let trx = db._createTransaction(opts);
      let result = trx.status();
      assertEqual("running", result.status);

      for (let i = 0; i < 12; ++i) {
        require("internal").sleep(1.0);
      
        result = trx.status();
        assertEqual("running", result.status);
      }
      // we need to add a bit of overhead here, as the garbage collection
      // for expired transactions may take a while until it aborts our trx
      
      result = trx.abort();
      assertEqual("aborted", result.status);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();

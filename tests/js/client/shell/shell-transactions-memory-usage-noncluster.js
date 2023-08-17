/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
  
const cn = "UnitTestsCollection";

function MemoryUsageSuite () {
  'use strict';
      
  let clearTransactions = () => {
    arango.DELETE("/_api/transaction/history");
  };
  
  let getTransactions = (database = '_system', collections = [cn]) => {
    let trx = arango.GET("/_api/transaction/history");

    trx = trx.filter((trx) => {
      if (trx.type === 'internal') {
        return false;
      }
      if (trx.database !== database) {
        return false;
      }
      return (collections.length === 0 || collections.every((c) => trx.collections.includes(c)));
    });
    return trx;
  };
  
  return {
    setUp: function () {
      clearTransactions();
      db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testInsertSingleDocument: function () {
      db[cn].insert({});
      //print(getTransactions());
    },

  };
}
if (db._version(true)['details']['maintainer-mode'] === 'true') {
  jsunity.run(MemoryUsageSuite);
}
return jsunity.done();

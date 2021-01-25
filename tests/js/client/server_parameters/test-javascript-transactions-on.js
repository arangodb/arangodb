/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
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
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'javascript.transactions': "true",
    'javascript.allow-admin-execute': "false"
  };
}

const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const db = require('internal').db;
const FoxxManager = require('@arangodb/foxx/manager');
const path = require('path');
const internal = require('internal');
const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'execute-transaction');

require("@arangodb/test-helper").waitForFoxxInitialized();

function testSuite() {
  const cn = "UnitTestsTransaction";

  return {
    setUp : function() {
      db._drop(cn);
      db._create(cn);
    },

    tearDown : function() {
      db._drop(cn);
    },

    testJavaScriptTransaction : function() {
      let res = db._executeTransaction({ 
        collections: { read: cn },
        action: function() { return "ok!"; },
      });
      assertEqual("ok!", res);
    },
    
    testJavaScriptTransactionFromFoxx : function() {
      const mount = '/test';

      FoxxManager.install(basePath, mount);
      let res = arango.GET(`/_db/_system/${mount}/execute`);
      assertEqual({ count: 1 }, res);
    },
    
    testJavaScriptTransactionViaAdminExecute : function() {
      let body = `require('@arangodb').db._executeTransaction({ collections: { read: "${cn}" }, action: function() {} }); return "ok!"; `;

      let res = arango.POST('/_db/_system/_admin/execute', body);
      // /_admin/execute API is turned off
      assertEqual(404, res.code);
    },

    testNonJavaScriptTransaction : function() {
      // non-JavaScript transactions should not be affected by the setting
      const opts = {
        collections: { write: cn },
      };
      
      const trx = db._createTransaction(opts);
      try {
        const tc = trx.collection(cn);
        assertEqual(0, tc.count());
        for (let i = 0; i < 10; ++i) {
          tc.insert({ _key: "test" + i });
        }
        assertEqual(10, tc.count());
        assertEqual(0, db[cn].count());
        trx.commit();
      
        assertEqual(10, db[cn].count());
      } catch (err) {
        trx.abort();
        throw err;
      }
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();

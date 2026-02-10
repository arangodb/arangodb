/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertEqual */

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
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" : "application/x-velocypack";
const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const ArangoError = arangodb.ArangoError;

let api = "/_api/transaction";

////////////////////////////////////////////////////////////////////////////////
// JS transactions via REST should return 501
////////////////////////////////////////////////////////////////////////////////
function jsTransactionRemovedSuite () {
  return {
    test_post_transaction_with_valid_body_returns_501: function() {
      let body = {
        collections: {},
        action: "function () { return 42; }"
      };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_IMPLEMENTED.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_IMPLEMENTED.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_NOT_IMPLEMENTED.code);
      assertTrue(doc.parsedBody['errorMessage'].indexOf('streaming transactions') !== -1);
    },

    test_post_transaction_with_empty_body_returns_501: function() {
      let doc = arango.POST_RAW(api, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_IMPLEMENTED.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_IMPLEMENTED.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_NOT_IMPLEMENTED.code);
    },

    test_client_executeTransaction_throws_error: function() {
      try {
        db._executeTransaction({
          collections: {},
          action: function () { return 1; }
        });
        fail();
      } catch (err) {
        assertTrue(err instanceof ArangoError);
        assertEqual(err.errorNum, internal.errors.ERROR_NOT_IMPLEMENTED.code);
        assertTrue(err.errorMessage.indexOf('streaming transactions') !== -1);
      }
    },

    test_wrong_method_still_returns_405: function() {
      let doc = arango.PATCH_RAW(api, {});
      assertEqual(doc.code, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
// Streaming transactions still work
////////////////////////////////////////////////////////////////////////////////
function streamingTransactionsStillWorkSuite () {
  let cn = "UnitTestsTransactions";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_streaming_transaction_begin_commit: function() {
      let beginDoc = arango.POST_RAW(api + "/begin", {
        collections: { write: [cn] }
      });

      assertEqual(beginDoc.code, 201);
      assertEqual(beginDoc.parsedBody['error'], false);
      let trxId = beginDoc.parsedBody.result.id;
      assertTrue(trxId !== undefined);

      // check status
      let statusDoc = arango.GET_RAW(api + "/" + trxId);
      assertEqual(statusDoc.code, 200);
      assertEqual(statusDoc.parsedBody.result.status, "running");

      // commit
      let commitDoc = arango.PUT_RAW(api + "/" + trxId, {});
      assertEqual(commitDoc.code, 200);
      assertEqual(commitDoc.parsedBody.result.status, "committed");
    },
  };
}

jsunity.run(jsTransactionRemovedSuite);
jsunity.run(streamingTransactionsStillWorkSuite);
return jsunity.done();

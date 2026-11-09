/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

// these tests cover CommTask::checkAuthEarly(): authentication now happens as
// soon as headers are parsed, before the body is read, for both HTTP/1.1 and
// HTTP/2. a rejected request must still get a normal, explained 401 response
// (not a reset connection), and the connection must stay usable afterwards.

const jsunity = require('jsunity');
const arango = require('@arangodb').arango;

const badAuth = {authorization: 'Bearer this.is.not.a.valid.jwt'};

function EarlyAuthCheckSuite() {
  'use strict';

  return {
    setUp: function () {
      arango.reconnect(arango.getEndpoint(), '_system', 'root', '');
    },

    testBadAuthRejectedWithExplainedResponse: function () {
      let res = arango.GET_RAW('/_api/version', badAuth);
      assertEqual(401, res.code);
      assertTrue(res.parsedBody.hasOwnProperty('errorMessage'));
      // same message handleAuthorizationChecks() used to send once the body
      // had already been read; early rejection must not change it
      assertEqual('User not authenticated', res.parsedBody.errorMessage);
    },

    // regresses the keep-alive race: sending the rejection too early (before
    // on_message_complete pauses the parser) used to force-close the
    // connection regardless of what the client asked for
    testConnectionStaysUsableAfterRejection: function () {
      let rejected = arango.GET_RAW('/_api/version', badAuth);
      assertEqual(401, rejected.code);

      let ok = arango.GET_RAW('/_api/version');
      assertEqual(200, ok.code);
      assertEqual('arango', ok.parsedBody.server);
    },

    testExemptPathSkipsEarlyAuthCheck: function () {
      // not in the checkAuthEarly skip-list exemption would normally see 401
      let res = arango.GET_RAW('/_admin/server/availability', badAuth);
      assertNotEqual(401, res.code);
      assertTrue(res.code === 200 || res.code === 503,
                `unexpected status for an auth-exempt path: ${res.code}`);
    },

    // a client that uses "Expect: 100-continue" must get the final 401
    // immediately, not a "100 Continue" followed by the rejection -- the
    // latter would desync a keep-alive connection for whoever reuses it next
    testExpectContinueWithBadAuthIsRejectedPromptly: function () {
      let headers = Object.assign({Expect: '100-continue'}, badAuth);
      let res = arango.POST_RAW('/_api/version', '', headers);
      assertEqual(401, res.code);

      let ok = arango.GET_RAW('/_api/version');
      assertEqual(200, ok.code);
      assertEqual('arango', ok.parsedBody.server);
    },

    testNormalAuthenticatedRequestStillWorks: function () {
      let res = arango.GET_RAW('/_api/version');
      assertEqual(200, res.code);
      assertFalse(res.parsedBody.hasOwnProperty('errorMessage'));
    },
  };
}

// H2CommTask::sendResponse() unconditionally decrements _numProcessing, which
// is only ever incremented in processRequest(). the early-rejection path
// skips processRequest() entirely, so it needs its own matching increment --
// otherwise the counter underflows and the connection's idle-timeout check
// never fires again. what's observable from a client isn't the counter
// itself but whether the connection keeps serving requests normally
// afterwards (an assertion failure on the underflow would otherwise take
// the whole connection down in maintainer/ASan builds).
//
// note: arango's HTTP client issues one request at a time and waits for the
// response, so this exercises the rejected-then-healthy-stream sequence
// per connection but not literal protocol-level concurrency of streams.
function EarlyAuthCheckH2Suite() {
  'use strict';

  return {
    setUp: function () {
      arango.reconnect(arango.getEndpoint(), '_system', 'root', '');
    },

    testRejectedStreamDoesNotBreakConnectionForLaterStreams: function () {
      if (arango.protocol() !== 'http2') {
        return;
      }

      let rejected = arango.GET_RAW('/_api/version', badAuth);
      assertEqual(401, rejected.code);

      let ok1 = arango.GET_RAW('/_api/version');
      assertEqual(200, ok1.code);

      let ok2 = arango.GET_RAW('/_api/version');
      assertEqual(200, ok2.code);
    },
  };
}

jsunity.run(EarlyAuthCheckSuite);
jsunity.run(EarlyAuthCheckH2Suite);

return jsunity.done();

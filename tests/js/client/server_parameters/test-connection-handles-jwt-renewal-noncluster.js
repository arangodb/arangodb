/*jshint globalstrict:false, strict:false */
/* global getOptions, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

const { versionHas } = require('@arangodb/test-helper');

// sanitizer and coverage builds of arangosh and the SUT react much slower, so
// their tokens live longer, lest they expire before a test expects them to.
// arangosh and the SUT come from the same build
const isInstrumented = versionHas('asan') || versionHas('tsan') || versionHas('coverage');
const tokenLifetimeSeconds = isInstrumented ? 15 : 5;
// how long a test waits for its tokens to expire
const tokenExpirySeconds = tokenLifetimeSeconds + (isInstrumented ? 5 : 1);
const jwtSecret = 'haxxmann';

if (getOptions === true) {
  return {
    'server.session-timeout': String(tokenLifetimeSeconds),
    'server.authentication': 'true',
    'server.jwt-secret': jwtSecret,
  };
}

const jsunity = require('jsunity');
const { assertEqual, assertNotEqual, assertTrue, assertFalse } = jsunity.jsUnity.assertions;
const internal = require('internal');
const users = require('@arangodb/users');
const IM = global.instanceManager;

// arangosh logs in via /_open/auth and keeps the resulting user token per
// connection handle. these tests check that the handles stay stable and that
// every handle renews its own token, as its own user, at its own server
function connectionHandlesJwtRenewalSuite() {
  'use strict';
  const otherUser = 'UnitTestsHandleUser';
  const otherPassword = 'handlepw';
  const changingUser = 'UnitTestsChangingUser';

  const connectAs = (username, password) => {
    arango.reconnect(IM.endpoint, '_system', username, password);
    return arango.getConnectionHandle();
  };
  const connectAsRoot = () => connectAs('root', '');
  const connectAsOther = () => connectAs(otherUser, otherPassword);

  // root may read the root user. every other user may only read itself,
  // unless it has rw access to _system, which makes it an admin
  const readRootUser = () => arango.GET_RAW('/_api/user/root').code;

  const handleTable = () => arango.getConnectionHandleTable();

  const waitForTokenExpiry = () => internal.sleep(tokenExpirySeconds);

  return {
    setUpAll: function () {
      IM.rememberConnection();
      connectAsRoot();
      users.save(otherUser, otherPassword);
      users.grantDatabase(otherUser, '_system', 'ro');
    },

    tearDownAll: function () {
      connectAsRoot();
      users.remove(otherUser);
      try {
        users.remove(changingUser);
      } catch (err) {
        // only created by one test
      }
      IM.reconnectMe();
    },

    setUp: function () {
      connectAsRoot();
    },

    // the handle identifies endpoint and user, not the token: logging in again
    // issues a new token, but yields the same handle and replaces the old
    // connection instead of adding one
    testReconnectingAsSameUserKeepsHandleAndTableSize: function () {
      const rootHandle = connectAsRoot();
      const otherHandle = connectAsOther();
      const tableSize = Object.keys(handleTable()).length;

      for (let i = 0; i < 5; ++i) {
        assertEqual(rootHandle, connectAsRoot());
        assertEqual(otherHandle, connectAsOther());
      }

      assertEqual(tableSize, Object.keys(handleTable()).length,
                  JSON.stringify(handleTable()));
    },

    testDifferentUsersGetDifferentHandles: function () {
      const rootHandle = connectAsRoot();
      const otherHandle = connectAsOther();

      assertNotEqual(rootHandle, otherHandle);
      const table = handleTable();
      assertTrue(table.hasOwnProperty(rootHandle), JSON.stringify(table));
      assertTrue(table.hasOwnProperty(otherHandle), JSON.stringify(table));
    },

    testHandleTableHasExactlyOneActiveAndNoUnidentifiedEntry: function () {
      connectAsRoot();
      connectAsOther();
      connectAsRoot();

      const table = handleTable();
      assertFalse(table.hasOwnProperty(''), JSON.stringify(table));
      const active = Object.keys(table).filter((handle) => table[handle].active);
      assertEqual([arango.getConnectionHandle()], active, JSON.stringify(table));
    },

    // a renewed token replaces the connection, but keeps its handle
    testRenewalKeepsHandleAndReplacesToken: function () {
      const handle = connectAsRoot();
      const tokenBefore = handleTable()[handle].jwtToken;
      assertEqual(200, readRootUser());

      waitForTokenExpiry();

      assertEqual(200, readRootUser());
      assertEqual(handle, arango.getConnectionHandle());
      const tokenAfter = handleTable()[handle].jwtToken;
      assertNotEqual(tokenBefore, tokenAfter);
    },

    // switching back to a cached handle whose token expired meanwhile must
    // renew that token before the next request, rather than sending the
    // expired one (which the server answers with HTTP 401)
    testSwitchingToHandleWithExpiredTokenRenewsIt: function () {
      const rootHandle = connectAsRoot();
      connectAsOther();

      waitForTokenExpiry();

      assertTrue(arango.connectHandle(rootHandle));
      assertEqual(200, readRootUser());
      assertEqual(rootHandle, arango.getConnectionHandle());
    },

    // every handle renews as the user it was created for, not as the user
    // that logged in last
    testRenewalUsesTheCredentialsOfTheHandle: function () {
      const rootHandle = connectAsRoot();
      const otherHandle = connectAsOther();

      waitForTokenExpiry();

      assertTrue(arango.connectHandle(rootHandle));
      assertEqual(200, readRootUser());

      assertTrue(arango.connectHandle(otherHandle));
      assertEqual(403, readRootUser());
      assertEqual(200, arango.GET_RAW('/_api/user/' + otherUser).code);

      assertTrue(arango.connectHandle(rootHandle));
      assertEqual(200, readRootUser());
    },

    // getEndpoint() and connectedUser() describe the active handle, also
    // after connectHandle() and after renewing its token. the second user
    // connects via another spelling of the same endpoint to tell them apart
    testEndpointAndUserFollowTheActiveHandle: function () {
      const otherEndpoint = 'http+' + IM.endpoint;
      const rootHandle = connectAsRoot();
      arango.reconnect(otherEndpoint, '_system', otherUser, otherPassword);
      const otherHandle = arango.getConnectionHandle();
      assertEqual(otherEndpoint, arango.getEndpoint());
      assertEqual(otherUser, arango.connectedUser());

      assertTrue(arango.connectHandle(rootHandle));
      assertEqual(IM.endpoint, arango.getEndpoint());
      assertEqual('root', arango.connectedUser());

      waitForTokenExpiry();

      assertEqual(200, readRootUser());
      assertEqual(IM.endpoint, arango.getEndpoint());
      assertEqual('root', arango.connectedUser());

      assertTrue(arango.connectHandle(otherHandle));
      assertEqual(403, readRootUser());
      assertEqual(otherEndpoint, arango.getEndpoint());
      assertEqual(otherUser, arango.connectedUser());
    },

    // a failed renewal keeps the expired token, so requests are refused; it
    // neither switches the handle's user nor breaks later logins
    testFailedRenewalIsReportedAndRecoverable: function () {
      users.save(changingUser, 'before');
      users.grantDatabase(changingUser, '_system', 'rw');
      const changingHandle = connectAs(changingUser, 'before');
      assertEqual(200, arango.GET_RAW('/_api/user/' + changingUser).code);

      connectAsRoot();
      users.update(changingUser, 'after');

      waitForTokenExpiry();

      assertTrue(arango.connectHandle(changingHandle));
      assertEqual(401, arango.GET_RAW('/_api/user/' + changingUser).code);
      // the next renewal attempt is postponed, the request fails right away
      assertEqual(401, arango.GET_RAW('/_api/user/' + changingUser).code);
      assertEqual(changingUser, arango.connectedUser());

      assertEqual(changingHandle, connectAs(changingUser, 'after'));
      assertEqual(200, arango.GET_RAW('/_api/user/' + changingUser).code);

      connectAsRoot();
      users.remove(changingUser);
    },

    // a token that is still valid is renewed via /_open/auth/renew without
    // sending the password, so a changed password does not matter until the
    // token would have expired
    testValidTokenIsRenewedWithoutPassword: function () {
      users.save(changingUser, 'before');
      users.grantDatabase(changingUser, '_system', 'rw');
      try {
        const changingHandle = connectAs(changingUser, 'before');
        connectAsRoot();
        users.update(changingUser, 'after');
        assertTrue(arango.connectHandle(changingHandle));

        // past half of the token lifetime renewal is due, but the token is
        // still valid
        internal.sleep(tokenLifetimeSeconds * 0.7);
        assertEqual(200, arango.GET_RAW('/_api/user/' + changingUser).code);

        // the original token has expired by now, the renewed one has not
        internal.sleep(tokenLifetimeSeconds * 0.5);
        assertEqual(200, arango.GET_RAW('/_api/user/' + changingUser).code);
        assertEqual(changingHandle, arango.getConnectionHandle());
      } finally {
        connectAsRoot();
        users.remove(changingUser);
      }
    },

    // with the jwt secret arangosh authenticates with a superuser token. it
    // does not expire, so reconnecting reuses it, and with it the cached
    // connection instead of opening a new one
    testReconnectWithJwtSecretReusesConnection: function () {
      const connectWithSecret = () => {
        arango.reconnect(IM.endpoint, '_system', undefined, undefined, false, jwtSecret);
        return arango.getConnectionHandle();
      };
      const handle = connectWithSecret();
      const { localPort, jwtToken } = handleTable()[handle];
      assertEqual(200, readRootUser());

      // a new token would differ in its "iat" claim, which counts seconds
      internal.sleep(1.1);
      assertEqual(handle, connectWithSecret());
      assertEqual(jwtToken, handleTable()[handle].jwtToken);
      assertEqual(localPort, handleTable()[handle].localPort);

      // also after switching to another handle and back
      connectAsRoot();
      assertEqual(handle, connectWithSecret());
      assertEqual(jwtToken, handleTable()[handle].jwtToken);
      assertEqual(localPort, handleTable()[handle].localPort);
      assertEqual(200, readRootUser());
    },

    // reusing a cached connection must not bring back how it was described
    // when it was created: getEndpoint() reports the spelling of the latest
    // reconnect. the superuser token is reused, so the cache always hits
    testCachedConnectionReportsLatestEndpointSpelling: function () {
      const otherEndpoint = 'http+' + IM.endpoint;
      const connectWithSecret = (endpoint) => {
        arango.reconnect(endpoint, '_system', undefined, undefined, false, jwtSecret);
        return arango.getConnectionHandle();
      };
      const handle = connectWithSecret(IM.endpoint);
      const { localPort } = handleTable()[handle];
      connectAsRoot();

      assertEqual(handle, connectWithSecret(otherEndpoint));
      assertEqual(localPort, handleTable()[handle].localPort);
      assertEqual(otherEndpoint, arango.getEndpoint());

      connectAsRoot();
      assertEqual(handle, connectWithSecret(IM.endpoint));
      assertEqual(IM.endpoint, arango.getEndpoint());
    },

    // a failed login neither drops nor retargets the active connection
    testFailedReconnectKeepsActiveConnection: function () {
      const rootHandle = connectAsRoot();

      let failed = false;
      try {
        arango.reconnect(IM.endpoint, '_system', otherUser, 'wrong password');
      } catch (err) {
        failed = true;
      }

      assertTrue(failed);
      assertEqual(rootHandle, arango.getConnectionHandle());
      assertEqual(IM.endpoint, arango.getEndpoint());
      assertEqual('root', arango.connectedUser());
      assertEqual(200, readRootUser());
    },

    testDisconnectedHandleIsGone: function () {
      const rootHandle = connectAsRoot();
      connectAsOther();

      assertTrue(arango.disconnectHandle(rootHandle));
      assertFalse(handleTable().hasOwnProperty(rootHandle));

      let failed = false;
      try {
        arango.connectHandle(rootHandle);
      } catch (err) {
        failed = true;
      }
      assertTrue(failed);

      assertEqual(rootHandle, connectAsRoot());
      assertEqual(200, readRootUser());
    },
  };
}

jsunity.run(connectionHandlesJwtRenewalSuite);
return jsunity.done();

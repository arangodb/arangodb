/*jshint globalstrict:false, strict:true */
/*global global */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for client-specific functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2016 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const { asyncRequest, getId, wait, drop } = global.ArangoClusterComm;
const { id } = global.ArangoServerState;
const { db } = require("@arangodb");
const { assertEqual } = jsunity.jsUnity.assertions;

function clusterCommAsyncRequestSuite() {
    // We use this URL as it accepts a POST, but does
    // not care at all for the body.
    // We can easily modify this.
    const url = '/_api/foxx/_local/heal';
    const method = "POST";

    // Just send request to ourself
    const target = `server:${id()}`;
    const dbName = db._name();

    const sendRequest = (body) => {
      const identifier = {coordTransactionID: getId()};
      asyncRequest(
        method,
        target,
        dbName,
        url,
        body,
        {},
        identifier
      );
      return identifier;
    };

    const awaitResponse = (identifier) => {
      try {
        return wait(identifier);
      } finally {
        drop(identifier);
      }
    };

    const Test = {};

    const bodies = [
      undefined,
      {},
      {a:1},
      [],
      [1],
      123,
      "foo",
      9.5,
      true
    ];

    for (const body of bodies) {
      // Plain
      Test[`testSend${JSON.stringify(body)}`] = function () {
        const ident = sendRequest(body);
        const result = awaitResponse(ident);
        assertEqual(result.code, 204);
      };

      Test[`testSendStringified${JSON.stringify(body)}`] = function () {
        const ident = sendRequest(JSON.stringify(body));
        const result = awaitResponse(ident);
        assertEqual(result.code, 204);
      };
    }


    return Test;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(clusterCommAsyncRequestSuite);

return jsunity.done();
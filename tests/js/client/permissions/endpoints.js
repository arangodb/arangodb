/*jshint globalstrict:false, strict:false */
/* global fail, arango, getOptions, assertTrue, assertEqual, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief teardown for dump/reload tests
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
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'javascript.endpoints-blacklist': [
      'tcp://127.0.0.1:8888',     // Will match http:// 
      '127.0.0.1:8899',           // will match at http and https.
      'ssl://127.0.0.1:7777',     // will match https://
      'arangodb.org',             // will match https + http
      'http://127.0.0.1:9999'            // won't match at all.
    ],
    'javascript.endpoints-whitelist': [
      'white.arangodb.org',
      'arangodb.com',             // will match https + http
    ]
  };
}

var jsunity = require('jsunity');

function testSuite() {
  const download = require('internal').download;
  let env = require('process').env;
  let arangodb = require("@arangodb");

  function downloadForbidden(url, method) {
    try {
      let reply = download(url, '', { method: method, timeout: 3 } );
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'while fetching: ' + url);
    }
  }

  function downloadPermitted(url, method) {
    try {
      let reply = download(url, '', { method: method, timeout: 30 } );
      if (reply.code === 200) {
        assertEqual(reply.code, 200);
      } else {
        assertEqual(reply.code, 500);
        assertTrue(reply.message.search('Could not connect') >=0 );
      }
    } catch (err) {
      assertNotEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'while fetching: ' + url + " Detail error: " + JSON.stringify(err) + ' ');
    }
  }

  function reconnectForbidden(url, method) {
    try {
      arango.reconnect(url, '_system', 'open', 'sesame');
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'while reconnecting: ' + url);
    }
  }

  function reconnectPermitted(url, method) {
    try {
      arango.reconnect(url, '_system', 'open', 'sesame');
      fail();
    } catch (err) {
      assertNotEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'while reconnecting: ' + url + " Detail error: " + JSON.stringify(err) + ' ');
      // we expect that we aren't able to connect these URLs...
      assertEqual(arangodb.ERROR_BAD_PARAMETER, err.errorNum, 'while reconnecting: ' + url + " Detail error: " + JSON.stringify(err) + ' ');
      
    }
  }

  return {
    testDownload : function() {
      // The filter will only match the host part. We specify one anyways.
      downloadForbidden('http://127.0.0.1:8888/testbla', 'GET');
      downloadForbidden('http://127.0.0.1:8888/testbla', 'POST');
      downloadForbidden('http://127.0.0.1:8899/testbla', 'GET');
      downloadForbidden('https://127.0.0.1:7777/testbla', 'GET');
      downloadForbidden('https://127.0.0.1:7777', 'GET');
      downloadForbidden('https://127.0.0.1:777/testbla', 'GET');
      downloadForbidden('http://arangodb.org/testbla', 'GET');
      downloadForbidden('https://arangodb.org/testbla', 'GET');
      downloadForbidden('http://heise.de', 'GET');
      downloadForbidden('http://127.0.0.1:9999', 'POST');

      downloadPermitted('https://white.arangodb.org/bla', 'GET');
      downloadPermitted('http://white.arangodb.org/bla', 'GET');
      downloadPermitted('https://arangodb.com/blog', 'GET');
      downloadPermitted('http://arangodb.com/blog', 'GET');

      reconnectForbidden('http://127.0.0.1:8888/testbla');
      reconnectForbidden('http://127.0.0.1:8888/testbla');
      reconnectForbidden('http://127.0.0.1:8899/testbla');
      reconnectForbidden('https://127.0.0.1:7777/testbla');
      reconnectForbidden('https://127.0.0.1:7777');
      reconnectForbidden('https://127.0.0.1:777/testbla');
      reconnectForbidden('http://arangodb.org/testbla');
      reconnectForbidden('https://arangodb.org/testbla');
      reconnectForbidden('http://heise.de');
      reconnectForbidden('http://127.0.0.1:9999');

      reconnectPermitted('https://white.arangodb.org/bla');
      reconnectPermitted('http://white.arangodb.org/bla');
      reconnectPermitted('https://arangodb.com/blog');
      reconnectPermitted('http://arangodb.com/blog');
    }
  };
}
jsunity.run(testSuite);
return jsunity.done();

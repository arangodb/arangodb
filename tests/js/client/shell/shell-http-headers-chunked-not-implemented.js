/*jshint globalstrict:false, strict:false */
/* global arango, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const request = require('@arangodb/request').request;

function headerWithChunkedEncodingSuite() {
  'use strict';

  var baseUrl = function() {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:');
  };

  // all requests with chunked encoding must not contain a content-length, so it cannot be added to the header
  return {

    testHeaderWithoutChunkedEncoding: function() {
      let result = request.post({
        url: baseUrl() + "/_api/cursor",
        headers: {
          'accept': 'application/json'
        },
        body: JSON.stringify({
          "query": "FOR p IN banana  RETURN p",
          "count": true,
          "batchSize": 2
        }), addContentLength: true
      });
      assertEqual(404, result.status);
    },

    testHeaderChunkedEncodingSimple: function() {
      let result = request.post({
        url: baseUrl() + "/_api/cursor",
        headers: {
          'accept': 'application/json', 'Transfer-Encoding': 'chunked', 'Content-Type': 'text/plain'
        },
        body: JSON.stringify({
          "query": "FOR p IN banana  RETURN p",
          "count": true,
          "batchSize": 2
        }), addContentLength: false
      });
      assertEqual(501, result.status);
    },

    testHeaderChunkedEncodingMultiple: function() {
      let result = request.post({
        url: baseUrl() + "/_api/cursor",
        headers: {'accept': 'application/json', 'Transfer-Encoding': 'gzip, chunked'},
        body: JSON.stringify({
          "query": "FOR p IN banana  RETURN p",
          "count": true,
          "batchSize": 2
        }), addContentLength: false
      });
      assertEqual(501, result.status);

      result = request.post({
        url: baseUrl() + "/_api/collection",
        headers: {
          'accept': 'application/json',
          'Transfer-Encoding': 'gzip, deflate, chunked',
          'addContentLength': false
        },
        body: JSON.stringify({
          "name": "testCollection"
        }), addContentLength: false
      });
      assertEqual(501, result.status);
    },

    testHeaderChunkedEncodingEmptyBody: function() {
      let result = request.post({
        url: baseUrl() + "/_api/cursor",
        headers: {'accept': 'application/json', 'Transfer-Encoding': 'gzip, chunked'},
        body: {}, addContentLength: false
      });
      assertEqual(501, result.status);
    },

  };
}

jsunity.run(headerWithChunkedEncodingSuite);
return jsunity.done();

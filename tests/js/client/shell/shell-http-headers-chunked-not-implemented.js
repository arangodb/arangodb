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
const _ = require("lodash");

function headerWithChunkedEncodingSuite() {
  'use strict';

  var baseUrl = function() {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };


  return {
    testHeaderChunkedEncodingSimple: function() {
      let result = request.post({
        url: baseUrl() + "/_api/cursor",
        headers: {'accept': 'application/json', 'Transfer-Encoding': 'chunked'},
        body: JSON.stringify({
          "query": "FOR p IN banana  RETURN p",
          "count": true,
          "batchSize": 2
        })
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
        })
      });
      assertEqual(501, result.status);

      result = request.post({
        url: baseUrl() + "/_api/collection",
        headers: {'accept': 'application/json', 'Transfer-Encoding': 'gzip, deflate, chunked'},
        body: JSON.stringify({
          "name": "testCollection"
        })
      });
      assertEqual(501, result.status);
    },

    testHeaderChunkedEncodingEmptyBody: function() {
      let result = request.post({
        url: baseUrl() + "/_api/cursor",
        headers: {'accept': 'application/json', 'Transfer-Encoding': 'gzip, chunked'},
        body: {}
      });
      assertEqual(501, result.status);
    },

  };
}

jsunity.run(headerWithChunkedEncodingSuite);
return jsunity.done();

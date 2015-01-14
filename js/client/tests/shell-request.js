/*global arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test request module
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triAGENS GmbH, Cologne, Germany
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
/// @author Alan Plum
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var expect = require('expect.js');
var arangodb = require('org/arangodb');
var request = require('org/arangodb/request');
var fs = require('fs');

// -----------------------------------------------------------------------------
// --SECTION--                                                           request
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function RequestSuite () {
  buildUrl = function (append) {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + '/_admin/echo' + append;
  };
  
  buildUrlBroken = function (append) {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + '/_not-there' + append;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test http DELETE
////////////////////////////////////////////////////////////////////////////////

    testDeleteWithDefaults: function () {
      var path = '/lol';
      var res = request.delete(buildUrl(path));
      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http GET
////////////////////////////////////////////////////////////////////////////////

    testGetWithDefaults: function () {
      var path = '/lol';
      var res = request.get(buildUrl(path));
      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http HEAD
////////////////////////////////////////////////////////////////////////////////

    testHeadWithDefaults: function () {
      var path = '/lol';
      var res = request.head(buildUrl(path));
      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.empty();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http POST
////////////////////////////////////////////////////////////////////////////////

    testPostWithDefaults: function () {
      var path = '/lol';
      var body = {hello: 'world'};
      var res = request.post(buildUrl(path), {body: body, json: true});
      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(JSON.parse(obj.requestBody)).to.eql(body);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http PATCH
////////////////////////////////////////////////////////////////////////////////

    testPatchWithDefaults: function () {
      var path = '/lol';
      var body = {hello: 'world'};
      var res = request.post(buildUrl(path), {body: body, json: true});
      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(JSON.parse(obj.requestBody)).to.eql(body);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http PUT
////////////////////////////////////////////////////////////////////////////////

    testPutWithDefaults: function () {
      var path = '/lol';
      var body = {hello: 'world'};
      var res = request.put(buildUrl(path), {body: body, json: true});
      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(JSON.parse(obj.requestBody)).to.eql(body);
    }

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RequestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

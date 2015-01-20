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
var url = require('url');
var qs = require('qs');

// -----------------------------------------------------------------------------
// --SECTION--                                                           request
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function RequestSuite () {
  buildUrl = function (append, base) {
    base = base === false ? '' : '/_admin/echo';
    append = append || '';
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + base + append;
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
      var res = request.post(buildUrl(path));
      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http request headers
////////////////////////////////////////////////////////////////////////////////

    testRequestHeaders: function () {
      var path = '/lol';
      var headers = {
        'content-type': 'application/x-magic',
        'content-disposition': 'x-chaotic; mood=cheerful',
        'x-hovercraft': 'full-of-eels'
      };
      var res = request.post(buildUrl(path), {headers: headers});
      expect(res).to.be.a(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('headers');
      Object.keys(headers).forEach(function (name) {
        expect(obj.headers[name]).to.equal(headers[name]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test 404
////////////////////////////////////////////////////////////////////////////////

    testError404: function () {
      var url = buildUrlBroken('/lol');
      expect(function () {
        request.get(url);
      }).to.throwException(function (err) {
        expect(err).to.have.property('message', 'Not Found');
        expect(err).to.have.property('statusCode', 404);
        expect(err).to.have.property('status', 404);
        expect(err).to.have.property('request');
        expect(err).to.have.property('response');
        expect(err.response).to.be.a(request.Response);
        expect(err.response).to.have.property('statusCode', 404);
        expect(err.response).to.have.property('status', 404);
        expect(err.request).to.have.property('url', url);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http auth
////////////////////////////////////////////////////////////////////////////////

    testBasicAuth: function () {
      var path = '/lol';
      var auth = {
        username: 'jcd',
        password: 'bionicman'
      };
      var res = request.post(buildUrl(path), {auth: auth});
      expect(res).to.be.a(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('headers');
      expect(obj.headers).to.have.property('authorization');
      expect(obj.headers.authorization).to.equal(
        'Basic ' + new Buffer(auth.username + ':' + auth.password).toString('base64')
      );
    },

    testBearerAuth: function () {
      var path = '/lol';
      var auth = {
        bearer: 'full of bears'
      };
      var res = request.post(buildUrl(path), {auth: auth});
      expect(res).to.be.a(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('headers');
      expect(obj.headers).to.have.property('authorization');
      expect(obj.headers.authorization).to.equal('Bearer ' + auth.bearer);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http request bodies
////////////////////////////////////////////////////////////////////////////////

    testJsonBody: function () {
      var path = '/lol';
      var reqBody = {
        hello: 'world',
        answer: 42,
        hovercraft: ['full', 'of', 'eels']
      };
      var res = request.post(buildUrl(path), {body: reqBody, json: true});
      expect(res).to.be.a(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(JSON.parse(obj.requestBody)).to.eql(reqBody);
    },

    testFormBody: function () {
      var path = '/lol';
      var reqBody = {
        hello: 'world',
        answer: '42',
        hovercraft: ['full', 'of', 'eels']
      };
      var res = request.post(buildUrl(path), {form: qs.stringify(reqBody)});
      expect(res).to.be.a(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(qs.parse(obj.requestBody)).to.eql(reqBody);
    },

    testStringBody: function () {
      var path = '/lol';
      var reqBody = 'hello world';
      var res = request.post(buildUrl(path), {body: reqBody});
      expect(res).to.be.a(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(obj.requestBody).to.equal(reqBody);
    },

    testBufferBody: function () {
      var path = '/lol';
      var reqBody = new Buffer('hello world');
      var headers = {'content-type': 'application/octet-stream'};
      var res = request.post(buildUrl(path), {body: reqBody, headers: headers});
      expect(res).to.be.a(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(obj.rawRequestBody).to.eql(reqBody.toJSON());
    },

    testBufferResponse: function () {
      var path = '/_admin/aardvark/favicon.ico';
      var res = request.get(buildUrl(path, false), {encoding: null});
      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.a(Buffer);
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

/*jshint globalstrict:false, strict:false */
/*global arango, Buffer */

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
var expect = require('chai').expect;
var request = require('@arangodb/request');
var url = require('url');
var querystring = require('querystring');
var qs = require('qs');


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function RequestSuite () {
  'use strict';
  var buildUrl = function (append, base) {
    base = base === false ? '' : '/_admin/echo';
    append = append || '';
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:') + base + append;
  };
  
  var buildUrlBroken = function (append) {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:') + '/_not-there' + append;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test http DELETE
////////////////////////////////////////////////////////////////////////////////

    testDeleteMethod: function () {
      var path = '/lol';
      var res = request.delete(buildUrl(path), {timeout: 300});

      expect(res).to.be.an.instanceof(request.Response);

      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http GET
////////////////////////////////////////////////////////////////////////////////

    testGetMethod: function () {
      var path = '/lol';
      var res = request.get(buildUrl(path), {timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj.headers).not.to.have.property('content-type');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http HEAD
////////////////////////////////////////////////////////////////////////////////

    testHeadMethod: function () {
      var path = '/lol';
      var res = request.head(buildUrl(path), {timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      expect(res.body).to.be.empty;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http POST
////////////////////////////////////////////////////////////////////////////////

    testPostMethod: function () {
      var path = '/lol';
      var res = request.post(buildUrl(path), {timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http PATCH
////////////////////////////////////////////////////////////////////////////////

    testPatchMethod: function () {
      var path = '/lol';
      var body = {hello: 'world'};
      var res = request.post(buildUrl(path), {body: body, json: true, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      expect(res.json).to.be.an('object');
      var obj = res.json;
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(JSON.parse(obj.requestBody)).to.eql(body);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http PUT
////////////////////////////////////////////////////////////////////////////////

    testPutMethod: function () {
      var path = '/lol';
      var body = {hello: 'world'};
      var res = request.put(buildUrl(path), {body: body, json: true, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      expect(res.json).to.be.an('object');
      var obj = res.json;
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
      var res = request.post(buildUrl(path), {headers: headers, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('headers');
      Object.keys(headers).forEach(function (name) {
        expect(obj.headers[name]).to.equal(headers[name]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test querystrings
////////////////////////////////////////////////////////////////////////////////

    testQuerystring: function () {
      var path = '/lol';
      var qstring = {
        hovercraft: ['full', 'of', 'eels']
      };
      var res = request.post(buildUrl(path), {qs: qstring, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      var urlObj = url.parse(obj.url);
      var query = qs.parse(urlObj.query);
      expect(query).to.eql(qstring);
    },

    testQuerystringWithUseQuerystring: function () {
      var path = '/lol';
      var qstring = {
        hovercraft: ['full', 'of', 'eels']
      };
      var res = request.post(buildUrl(path), {qs: qstring, useQuerystring: true, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      var urlObj = url.parse(obj.url);
      var query = querystring.parse(urlObj.query);
      expect(query).to.eql(qstring);
    },

    testQuerystringAsString: function () {
      var path = '/lol';
      var qstring = qs.stringify({
        hovercraft: ['full', 'of', 'eels']
      });
      var res = request.post(buildUrl(path), {qs: qstring, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      var urlObj = url.parse(obj.url);
      expect(urlObj.query).to.eql(qstring);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test http url object
////////////////////////////////////////////////////////////////////////////////

    testUrlObject: function () {
      var path = url.parse(buildUrl('/lol'));
      var res = request.post({url: path, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.url).to.equal(path.pathname);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test 404
////////////////////////////////////////////////////////////////////////////////

    test404: function () {
      var url = buildUrlBroken('/lol');
      var res = request.get(url, {timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('message', 'Not Found');
      expect(res).to.have.property('statusCode', 404);
      expect(res).to.have.property('status', 404);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bad json
////////////////////////////////////////////////////////////////////////////////

    testBadJson: function () {
      var url = buildUrl('/_admin/aardvark/img/ArangoDB-community-edition-Web-UI.png', false);
      var res = request.get(url, {json: true, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      expect(res.json).to.be.equal(undefined);
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
      var res = request.post(buildUrl(path), {auth: auth, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('headers');
      expect(obj.headers).to.have.property('authorization');
      expect(obj.headers.authorization).to.equal(
        'Basic ' + new Buffer(auth.username + ':' + auth.password).toString('base64')
      );
    },

    testBasicAuthViaUrl: function () {
      var path = '/lol';
      var auth = {
        username: 'jcd',
        password: 'bionicman'
      };
      var res = request.post(buildUrl(path).replace(/^(https?:\/\/)/, function (m) {
        return m + encodeURIComponent(auth.username) + ':' + encodeURIComponent(auth.password) + '@';
      }), {timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
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
      var res = request.post(buildUrl(path), {auth: auth, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('headers');
      expect(obj.headers).to.have.property('authorization');
      expect(obj.headers.authorization).to.equal('bearer ' + auth.bearer);
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
      var res = request.post(buildUrl(path), {body: reqBody, json: true, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      expect(res.json).to.be.an('object');
      var obj = res.json;
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
      var res = request.post(buildUrl(path), {form: reqBody, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(qs.parse(obj.requestBody)).to.eql(reqBody);
    },

    testFormBodyWithQuerystring: function () {
      var path = '/lol';
      var reqBody = {
        hello: 'world',
        answer: '42',
        hovercraft: ['full', 'of', 'eels']
      };
      var res = request.post(buildUrl(path), {form: reqBody, useQuerystring: true, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(querystring.parse(obj.requestBody)).to.eql(reqBody);
    },

    testFormBodyAsString: function () {
      var path = '/lol';
      var reqBody = {
        hello: 'world',
        answer: '42',
        hovercraft: ['full', 'of', 'eels']
      };
      var res = request.post(buildUrl(path), {form: qs.stringify(reqBody), timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(qs.parse(obj.requestBody)).to.eql(reqBody);
    },

    testStringBody: function () {
      var path = '/lol';
      var reqBody = 'hello world';
      var res = request.post(buildUrl(path), {body: reqBody, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(obj.requestBody).to.equal(reqBody);
    },

    testBufferBody: function () {
      var path = '/lol';
      var reqBody = new Buffer('hello world');
      var headers = {'content-type': 'application/octet-stream'};
      var res = request.post(buildUrl(path), {body: reqBody, headers: headers, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      var obj = JSON.parse(res.body);
      expect(obj.path).to.equal(path);
      expect(obj).to.have.property('requestBody');
      expect(obj.rawRequestBody).to.eql(reqBody.toJSON());
    },

    testBufferResponse: function () {
      var path = '/_admin/aardvark/favicon.ico';
      var res = request.get(buildUrl(path, false), {encoding: null, timeout: 300});
      expect(res).to.be.an.instanceof(request.Response);
      expect(res.body).to.be.an.instanceof(Buffer);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RequestSuite);

return jsunity.done();


/*jshint globalstrict:false, strict:false */
/*global arango, Buffer, VPACK_TO_V8, V8_TO_VPACK */

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
/// @author Jan Christoph Uhde
/// @author Copyright 2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

var jsunity = require('jsunity');
var expect = require('expect.js');
var request = require('@arangodb/request');
var url = require('url');
var querystring = require('querystring');
var qs = require('qs');

var buildUrl = function (append, base) {
  base = base === false ? '' : '/_admin/echo';
  append = append || '';
  return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + base + append;
};

var buildUrlBroken = function (append) {
  return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + '/_not-there' + append;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function RequestSuite () {
  return { 
    testVersionJsonJson : verionJsonJson,
    testVersionVpackJson: verionVpackJson,
    testVersionJsonVpack: versionJsonVpack,
    testVersionVpackVpack: versionVpackVpack,
    testEchoVpackVpack: echoVpackVpack
  };
};


function verionJsonJson() {
  var path = '/_api/version';
  var headers = {
    'content-type': 'application/json',
    'accept'      : 'application/json'
  };

  var res = request.post(buildUrl(path, false), {headers : headers, timeout: 300});

  expect(res).to.be.a(request.Response);
  expect(res.body).to.be.a('string');
  expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
  expect(String(res.headers['content-type'])).to.have.string("application/json");

  var obj = JSON.parse(res.body);
  var expected = { "server" : "arango" , "version" : "3.0.devel" };
  expect(obj).to.eql(expected); //eql compares deep
};

function verionVpackJson() {
  var path = '/_api/version';
  var headers = {
    'content-type': 'application/x-velocypack',
    'accept'      : 'application/json'
  };

  var res = request.post(buildUrl(path, false), {headers : headers, timeout: 300});

  expect(res).to.be.a(request.Response);
  expect(res.body).to.be.a('string');
  expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
  expect(String(res.headers['content-type'])).to.have.string("application/json");

  var obj = JSON.parse(res.body);
  var expected = { "server" : "arango" , "version" : "3.0.devel" };
  expect(obj).to.eql(expected); //eql compares deep
};

function versionJsonVpack () {
  var path = '/_api/version';
  var headers = {
    'content-type': 'application/json',
    'accept'      : 'application/x-velocypack'
  };

  var res = request.post(buildUrl(path,false), {headers : headers, timeout: 300});

  expect(res).to.be.a(request.Response);
  expect(res.body).to.be.a('string');
  expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
  expect(String(res.headers['content-type'])).to.have.string("application/x-velocypack");

  var obj = VPACK_TO_V8(res.body);
  var expected = { "server" : "arango" , "version" : "3.0.devel" };
  expect(obj).to.eql(expected); //eql compares deep
};

function versionVpackVpack () {
  var path = '/_api/version';
  var headers = {
    'content-type': 'application/x-velocypack',
    'accept'      : 'application/x-velocypack'
  };

  var res = request.post(buildUrl(path,false), {headers : headers, timeout: 300});

  expect(res).to.be.a(request.Response);
  expect(res.body).to.be.a('string');
  expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
  expect(String(res.headers['content-type'])).to.have.string("application/x-velocypack");

  var obj = VPACK_TO_V8(res.body);
  var expected = { "server" : "arango" , "version" : "3.0.devel" };
  expect(obj).to.eql(expected); //eql compares deep
};

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

function echoVpackVpack () {
  var path = '/_admin/echo';
  var headers = {
    'content-type': 'application/x-velocypack',
    'accept'      : 'application/x-velocypack'
  };

  var obj = { "server" : "arango" , "version" : "3.0.devel" };
  var body = V8_TO_VPACK(obj);
  var res = request.post(buildUrl(path),{ headers : headers, body : body, timeout: 300});

  expect(res).to.be.a(request.Response);
  expect(res.body).to.be.a('string');
  expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
  //var obj = JSON.parse(res.body);
  //print_vpack_as_json(res.body);
  //expect(VPACK_TO_V8().to.equal();
};



////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RequestSuite);

return jsunity.done();

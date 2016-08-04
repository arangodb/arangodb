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
/// @author Jan Christoph Uhde
/// @author Copyright 2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var expect = require('expect.js');
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
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + base + append;
  };

  var buildUrlBroken = function (append) {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + '/_not-there' + append;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test http DELETE
////////////////////////////////////////////////////////////////////////////////

    testVersionJsonJson: function () {
      var path = '/_api/version';
      var headers = {
        'content-type': 'application/json',
        'accept'      : 'application/json'
      };
      //var res = request.post(buildUrl(path), {headers : headers, timeout: 300});
      var res = request.post(path, {headers : headers, timeout: 300});

      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var obj = JSON.parse(res.body);
      var expected = { "server" : "arango" , "version" : "3.0.x-devel" };
      //expect(JSON.stringify(obj)).to.equal(JSON.stringify(expected));
      expect(obj).to.eql(expected);
    },

//    testVersionVpackJson: function () {
//      var path = '/_api/version';
//      var headers = {
//        'content-type': 'application/x-velocypack',
//        'accept'      : 'application/json'
//      };
//      var res = request.post(buildUrl(path), {headers : headers, timeout: 300});
//
//      expect(res).to.be.a(request.Response);
//      expect(res.body).to.be.a('string');
//      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
//      var obj = JSON.parse(res.body);
//      expect(obj.path).to.equal(path);
//    },
//
    testVersionJsonVpack: function () {
      var path = '/_api/version';
      var headers = {
        'content-type': 'application/json',
        'accept'      : 'application/x-velocypack'
      };
      //var res = request.post(buildUrl(path,false), {headers : headers, timeout: 300});
      var res = request.post(path, {headers : headers, timeout: 300});

      expect(res).to.be.a(request.Response);
      expect(res.body).to.be.a('string');
      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
      var expected = "velocypack blablba echt scheisse" ;
      require("internal").print(JSON.stringify(res.body));
      require("internal").print(JSON.stringify(expected));
      //var obj = JSON.parse(res.body);
      expect(res.body).to.eql(expected);
    },
//
//    testVersionVpackVpack: function () {
//      var path = '/_api/version';
//      var headers = {
//        'content-type': 'application/x-velocypack',
//        'accept'      : 'application/x-velocypack'
//      };
//      var res = request.post(buildUrl(path), {headers : headers, timeout: 300});
//
//      expect(res).to.be.a(request.Response);
//      expect(res.body).to.be.a('string');
//      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
//      var obj = JSON.parse(res.body);
//      expect(obj.path).to.equal(path);
//    },
//
//    ///////////////////////////////////////////////////////////////////////////////////////
//    ///////////////////////////////////////////////////////////////////////////////////////
//
//    testVpackEcho: function () {
//      var path = '/_admin/echo';
//      var body = "foooo"
//      var res = request.post(buildUrl(path),{ headers : headers, body : body, timeout: 300});
//
//      expect(res).to.be.a(request.Response);
//      expect(res.body).to.be.a('string');
//      expect(Number(res.headers['content-length'])).to.equal(res.rawBody.length);
//      //var obj = JSON.parse(res.body);
//      expect(res.body).to.equal(body);
//    }



  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RequestSuite);

return jsunity.done();


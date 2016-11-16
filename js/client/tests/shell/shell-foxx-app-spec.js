/* global arango, describe, beforeEach, afterEach, it*/
'use strict';

var expect = require('chai').expect;
var FoxxManager = require('org/arangodb/foxx/manager');
var fs = require('fs');
var internal = require('internal');
var basePath = fs.makeAbsolute(fs.join(internal.startupPath, 'common', 'test-data', 'apps'));
var url = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');

describe('HTTP headers in Foxx apps', function () {
  describe('Check request-response', function () {
    var mount;

    beforeEach(function () {
      mount = '/unittest/headers';
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch (err) {
      }
      FoxxManager.install(basePath, mount);
    });

    afterEach(function () {
      FoxxManager.uninstall(mount, {force: true});
    });
    
    it("sends a CORS options request", function () {
      var opts = { headers: { "origin" : url }, method: "OPTIONS" };
      var result = internal.download(url + "/unittest/headers/header-echo", "", opts);
      expect(result.headers['allow']).to.eql('DELETE, GET, HEAD, PATCH, POST, PUT');
      expect(result.headers['access-control-expose-headers']).to.eql('etag, content-encoding, content-length, location, server, x-arango-errors, x-arango-async-id');
      expect(result.headers['access-control-allow-credentials']).to.eql('true');
      expect(result.headers['access-control-allow-origin']).to.eql(url);
      expect(result.headers['access-control-allow-methods']).to.eql('DELETE, GET, HEAD, PATCH, POST, PUT');
    });
    
    it("sends a CORS options request, overriding headers", function () {
      var opts = { headers: { "origin" : url, 'x-session-id' : 'abc' } };
      var result = internal.download(url + "/unittest/headers/header-cors", "", opts);
      expect(result.headers['access-control-expose-headers']).to.eql('x-session-id');
    });
    
    it("echoes back the headers sent", function () {
      var opts = { headers: { "x-test" : "abc", "x-xxxx" : "1234", "X-Testmann" : "1,2,3" } };
      var result = internal.download(url + "/unittest/headers/header-echo", "", opts);

      expect(result.headers['x-xxxx']).to.eql('1234');
      expect(result.headers['x-testmann']).to.eql('1,2,3');
      expect(result.headers['x-test']).to.eql('abc');
    });

    it("returns a static header", function () {
      var result = internal.download(url + "/unittest/headers/header-static");
      expect(result.headers['x-foobar']).to.eql('baz');
    });
    
  });
});

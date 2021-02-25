/* global arango, describe, beforeEach, afterEach, it*/
'use strict';

const expect = require('chai').expect;
const FoxxManager = require('org/arangodb/foxx/manager');
const fs = require('fs');
const internal = require('internal');
const arango = require('@arangodb').arango;
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps', 'headers'));
const origin = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:');

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('HTTP headers in Foxx services', function () {
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
      var opts = { headers: { origin }, method: "OPTIONS" };
      var result = internal.download(origin + "/unittest/headers/header-echo", "", opts);
      expect(result.code).to.equal(200);
      expect(result.headers['access-control-expose-headers']).to.equal('etag, content-encoding, content-length, location, server, x-arango-errors, x-arango-async-id');
      expect(result.headers).not.to.have.property('access-control-allow-headers');
      expect(result.headers['access-control-allow-credentials']).to.equal('true');
      expect(result.headers['access-control-allow-origin']).to.equal(origin);
      expect(result.headers['access-control-allow-methods']).to.equal('DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT');
    });

    it("exposes response headers automatically", function () {
      var result = internal.download(origin + "/unittest/headers/header-automatic", "", { headers: { origin }, method: "POST" });
      expect(result.code).to.equal(204);
      expect(result.headers['x-foobar']).to.equal('baz');
      expect(result.headers['x-nofoobar']).to.equal('baz');
      const irrelevantHeaders = ['http/1.1', 'connection', 'content-type', 'keep-alive'];
      expect(result.headers['access-control-expose-headers']).to.equal(Object.keys(result.headers).filter(x => !x.startsWith('x-content-type-options') && !x.startsWith('access-control-') && !irrelevantHeaders.includes(x)).sort().join(', '));
      expect(result.headers['access-control-allow-credentials']).to.equal('true');
    });

    it("exposes response headers manually", function () {
      var result = internal.download(origin + "/unittest/headers/header-manual", "", { headers: { origin }, method: "POST" });
      expect(result.code).to.equal(204);
      expect(result.headers['x-foobar']).to.equal('baz');
      expect(result.headers['x-nofoobar']).to.equal('baz');
      expect(result.headers['access-control-expose-headers']).to.equal('x-foobar');
      expect(result.headers['access-control-allow-credentials']).to.equal('false');
    });

    it("allows requested headers", function () {
      var opts = { headers: { origin, "access-control-request-headers" : "foo, bar" }, method: "OPTIONS" };
      var result = internal.download(origin + "/unittest/headers/header-echo", "", opts);
      expect(result.code).to.equal(200);
      expect(result.headers['access-control-allow-headers']).to.equal("foo, bar");
      expect(result.headers['access-control-allow-credentials']).to.equal('true');
      expect(result.headers['access-control-allow-origin']).to.equal(origin);
    });

    it("sets defaults for responses without headers", function () {
      var opts = { headers: { origin }, method: "POST" };
      var result = internal.download(origin + "/unittest/headers/header-empty", "", opts);
      const irrelevantHeaders = ['http/1.1', 'connection', 'content-type', 'keep-alive'];
      expect(result.headers['access-control-expose-headers']).to.equal(Object.keys(result.headers).filter(x => !x.startsWith('x-content-type-options') && !x.startsWith('access-control-') && !irrelevantHeaders.includes(x)).sort().join(', '));
      expect(result.headers['access-control-allow-credentials']).to.equal('true');
    });
  });
});

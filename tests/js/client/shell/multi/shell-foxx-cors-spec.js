/* global describe, beforeEach, afterEach, it*/
'use strict';

const expect = require('chai').expect;
const FoxxManager = require('org/arangodb/foxx/manager');
const fs = require('fs');
const internal = require('internal');
const arango = require('@arangodb').arango;
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps', 'headers'));
const origin = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^h2:/, 'http:');

const irrelevantHeaders = ['http/1.1', 'connection', 'content-type', 'content-length', 'keep-alive', 'server', 'allow',
  'x-arango-queue-time-seconds', 'content-security-policy', 'cache-control', 'pragma', 'expires', 'strict-transport-security'];
function filterIrrelevant(header) {
  return !header.startsWith('x-content-type-options') &&
    !header.startsWith('access-control-') &&
    !irrelevantHeaders.includes(header);
}
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
      var opts = { origin };
      var result = arango.OPTIONS_RAW("/unittest/headers/header-echo", "", opts);
      expect(result.code).to.equal(200);
      expect(result.headers['access-control-expose-headers']).to.equal('etag, content-encoding, content-length, location, server, x-arango-errors, x-arango-async-id');
      expect(result.headers).not.to.have.property('access-control-allow-headers');
      expect(result.headers['access-control-allow-credentials']).to.equal('false');
      expect(result.headers['access-control-allow-origin']).to.equal(origin);
      expect(result.headers['access-control-allow-methods']).to.equal('DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT');
    });

    it("exposes response headers automatically", function () {
      var result = arango.POST_RAW( "/unittest/headers/header-automatic", "", { origin });
      expect(result.code).to.equal(204);
      expect(result.headers['x-foobar']).to.equal('baz');
      expect(result.headers['x-nofoobar']).to.equal('baz');
      expect(result.headers['access-control-expose-headers'].
             split(', ').
             filter(filterIrrelevant).
             join(', ')
            ).to.equal(
              Object.keys(result.headers).
                filter(filterIrrelevant).
                sort().
                join(', '));
      expect(result.headers['access-control-allow-credentials']).to.equal('false');
    });

    it("exposes response headers manually", function () {
      var result = arango.POST_RAW("/unittest/headers/header-manual", "", { origin });
      expect(result.code).to.equal(204);
      expect(result.headers['x-foobar']).to.equal('baz');
      expect(result.headers['x-nofoobar']).to.equal('baz');
      expect(result.headers['access-control-expose-headers']).to.equal('x-foobar');
      expect(result.headers['access-control-allow-credentials']).to.equal('false');
    });

    it("allows requested headers", function () {
      var opts = { origin, "access-control-request-headers" : "foo, bar" };
      var result = arango.OPTIONS_RAW("/unittest/headers/header-echo", "", opts);
      expect(result.code).to.equal(200);
      expect(result.headers['access-control-allow-headers']).to.equal("foo, bar");
      expect(result.headers['access-control-allow-credentials']).to.equal('false');
      expect(result.headers['access-control-allow-origin']).to.equal(origin);
    });
    
    it("sets defaults for responses without headers", function () {
      var opts = { origin };
      var result = arango.POST_RAW("/unittest/headers/header-empty", "", opts);
      expect(result.headers['access-control-expose-headers'].
             split(', ').
             filter(filterIrrelevant).
             join(', ')
            ).to.equal(
              Object.keys(result.headers).
                filter(filterIrrelevant).
                sort().
                join(', '));
      expect(result.headers['access-control-allow-credentials']).to.equal('false');
    });
  });
});

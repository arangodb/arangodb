/*global describe, it */
'use strict';
const expect = require('chai').expect;
const SyntheticRequest = require('@arangodb/foxx/router/request');

function createNativeRequest(opts) {
  if (!opts) {
    opts = {};
  }
  const req = {
    requestType: opts.method || 'get',
    protocol: opts.protocol || 'http',
    database: opts.db || '_system',
    url: `${opts.mount || ''}/${opts.path}`,
    suffix: opts.path ? opts.path.split('/').filter(Boolean) : [],
    headers: opts.headers || {},
    server: opts.server || {
      address: '127.0.0.1',
      port: opts.port || 8529
    },
    client: opts.client || {
      address: '127.0.0.1',
      port: 33333
    }
  };
  return req;
}

describe('SyntheticRequest', function () {
  describe('protocol', function () {
    it('defaults to the protocol of the native request for http', function () {
      const rawReq = createNativeRequest({protocol: 'http'});
      const req = new SyntheticRequest(rawReq, {});
      expect(req.protocol).to.equal('http');
      expect(req.secure).to.equal(false);
    });
    it('defaults to the protocol of the native request for https', function () {
      const rawReq = createNativeRequest({protocol: 'https'});
      const req = new SyntheticRequest(rawReq, {});
      expect(req.protocol).to.equal('https');
      expect(req.secure).to.equal(true);
    });
    it('ignores x-forwarded-proto if trustProxy is false', function () {
      const rawReq = createNativeRequest({
        protocol: 'http',
        headers: {'x-forwarded-proto': 'https'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.protocol).to.equal('http');
      expect(req.secure).to.equal(false);
    });
    it('overrides http with x-forwarded-proto if trustProxy is true', function () {
      const rawReq = createNativeRequest({
        protocol: 'http',
        headers: {'x-forwarded-proto': 'https'}
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.protocol).to.equal('https');
      expect(req.secure).to.equal(true);
    });
    it('overrides https with x-forwarded-proto if trustProxy is true', function () {
      const rawReq = createNativeRequest({
        protocol: 'https',
        headers: {'x-forwarded-proto': 'http'}
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.protocol).to.equal('http');
      expect(req.secure).to.equal(false);
    });
  });
  describe('hostname and port', function () {
    it('defaults to the host header of the native request', function () {
      const rawReq = createNativeRequest({
        headers: {host: 'lolcat.example:1234'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.hostname).to.equal('lolcat.example');
      expect(req.port).to.equal(1234);
    });
    it('falls back to the server info of the native request if no host header is set', function () {
      const rawReq = createNativeRequest({
        server: {
          address: '127.1.1.1',
          port: 2345
        }
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.hostname).to.equal('127.1.1.1');
      expect(req.port).to.equal(2345);
    });
    it('defaults to port 80 if the host header has no port on http', function () {
      const rawReq = createNativeRequest({
        headers: {host: 'lolcat.example'},
        protocol: 'http'
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.port).to.equal(80);
    });
    it('defaults to port 443 if the host header has no port on https', function () {
      const rawReq = createNativeRequest({
        headers: {host: 'lolcat.example'},
        protocol: 'https'
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.port).to.equal(443);
    });
    it('ignores x-forwarded-host if trustProxy is false', function () {
      const rawReq = createNativeRequest({
        headers: {
          host: 'lolcat.example:1234',
          'x-forwarded-host': 'example.com:5678'
        }
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.hostname).to.equal('lolcat.example');
      expect(req.port).to.equal(1234);
    });
    it('overrides with x-forwarded-host if trustProxy is true', function () {
      const rawReq = createNativeRequest({
        headers: {
          host: 'lolcat.example:1234',
          'x-forwarded-host': 'doge.example.com:5678'
        }
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.hostname).to.equal('doge.example.com');
      expect(req.port).to.equal(5678);
    });
  });
  describe('remoteAddress and remoteAddresses', function () {
    it('defaults to the client info of the native request', function () {
      const rawReq = createNativeRequest({
        client: {
          address: '127.2.2.2',
          port: 1234
        }
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.remoteAddress).to.equal('127.2.2.2');
      expect(req.remoteAddresses).to.eql([req.remoteAddress]);
    });
    it('ignores x-forwarded-for if trustProxy is false', function () {
      const rawReq = createNativeRequest({
        client: {
          address: '127.3.3.3',
          port: 1234
        },
        headers: {'x-forwarded-for': '123.45.67.89'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.remoteAddress).to.equal('127.3.3.3');
      expect(req.remoteAddresses).to.eql([req.remoteAddress]);
    });
    it('overrides with x-forwarded-for if trustProxy is true', function () {
      const ip = '123.45.67.89';
      const rawReq = createNativeRequest({
        client: {
          address: '127.3.3.3',
          port: 1234
        },
        headers: {'x-forwarded-for': ip}
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.remoteAddress).to.equal(ip);
      expect(req.remoteAddresses).to.eql([ip]);
    });
    it('correctly handles x-forwarded-for proxy lists', function () {
      const ips = ['123.45.67.89', '1.2.3.4', '3.4.5.6', '7.8.9.1'];
      const rawReq = createNativeRequest({
        client: {
          address: '127.3.3.3',
          port: 1234
        },
        headers: {'x-forwarded-for': ips.join(', ')}
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.remoteAddress).to.equal(ips[0]);
      expect(req.remoteAddresses).to.eql(ips);
    });
  });
  describe('remotePort', function () {
    it('defaults to the client info of the native request', function () {
      const rawReq = createNativeRequest({
        client: {
          address: '127.1.1.1',
          port: 9001
        }
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.remotePort).to.equal(9001);
    });
    it('ignores x-forwarded-port if trustProxy is false', function () {
      const rawReq = createNativeRequest({
        client: {
          address: '127.1.1.1',
          port: 9001
        },
        headers: {'x-forwarded-port': 9999}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.remotePort).to.equal(9001);
    });
    it('overrides with x-forwarded-port if trustProxy is true', function () {
      const rawReq = createNativeRequest({
        client: {
          address: '127.1.1.1',
          port: 9001
        },
        headers: {'x-forwarded-port': 9999}
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.remotePort).to.equal(9999);
    });
  });
  it('TODO');
});

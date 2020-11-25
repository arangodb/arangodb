/* global describe, it */
'use strict';
const resolvePath = require('path').resolve;
const expect = require('chai').expect;
const crypto = require('@arangodb/crypto');
const SyntheticRequest = require('@arangodb/foxx/router/request');
const createNativeRequest = require('@arangodb/foxx/test-utils').createNativeRequest;

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('SyntheticRequest', function () {
  describe('protocol/secure', function () {
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
    it('ignores x-forwarded-proto if it is empty', function () {
      const rawReq = createNativeRequest({
        protocol: 'http',
        headers: {'x-forwarded-proto': ''}
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.protocol).to.equal('http');
      expect(req.secure).to.equal(false);
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

  describe('hostname/port', function () {
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
    it('ignores x-forwarded-host if it is empty', function () {
      const rawReq = createNativeRequest({
        headers: {
          host: 'lolcat.example:1234',
          'x-forwarded-host': ''
        }
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.hostname).to.equal('lolcat.example');
      expect(req.port).to.equal(1234);
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

  describe('remoteAddress(es)', function () {
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
    it('ignores x-forwarded-for if it is empty', function () {
      const rawReq = createNativeRequest({
        client: {
          address: '127.3.3.3',
          port: 1234
        },
        headers: {'x-forwarded-for': ''}
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.remoteAddress).to.equal('127.3.3.3');
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
    it('ignores x-forwarded-port if it is empty', function () {
      const rawReq = createNativeRequest({
        client: {
          address: '127.1.1.1',
          port: 9001
        },
        headers: {'x-forwarded-port': ''}
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.remotePort).to.equal(9001);
    });
    it('ignores x-forwarded-port if trustProxy is false', function () {
      const rawReq = createNativeRequest({
        client: {
          address: '127.1.1.1',
          port: 9001
        },
        headers: {'x-forwarded-port': '9999'}
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
        headers: {'x-forwarded-port': '9999'}
      });
      const req = new SyntheticRequest(rawReq, {trustProxy: true});
      expect(req.remotePort).to.equal(9999);
    });
  });

  describe('queryParams', function () {
    it('is correctly derived from the native request when empty', function () {
      const rawReq = createNativeRequest({});
      const req = new SyntheticRequest(rawReq, {});
      expect(req.queryParams).to.eql({});
    });
    it('is correctly derived from the native request', function () {
      const rawReq = createNativeRequest({
        search: '?a=1&b=2&c=3'
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.queryParams).to.eql({
        a: '1',
        b: '2',
        c: '3'
      });
    });
  });

  describe('baseUrl', function () {
    it('is correctly derived from the native request', function () {
      const rawReq = createNativeRequest({
        db: 'bananas'
      });
      const req = new SyntheticRequest(rawReq, {
        mount: '/hello'
      });
      expect(req.baseUrl).to.equal('/_db/bananas');
    });
  });

  describe('headers', function () {
    it('exposes the headers of the native request', function () {
      const headers = {};
      const rawReq = createNativeRequest({ headers });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.headers).to.equal(headers);
    });
  });

  describe('auth', function () {
    function btoa (str) {
      return new Buffer(str).toString("base64");
    }
    it('recognizes no auth', function () {
      const rawReq = createNativeRequest({ headers: {} });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.auth).to.eql(null);
    });
    it('recognizes bearer auth', function () {
      const headers = {authorization: "Bearer deadbeef"};
      const rawReq = createNativeRequest({ headers });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.auth).to.eql({bearer: "deadbeef"});
    });
    it('recognizes empty basic auth', function () {
      const headers = {authorization: `Basic ${btoa("")}`};
      const rawReq = createNativeRequest({ headers });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.auth).to.eql({basic: {}});
    });
    it('recognizes malformed basic auth', function () {
      const headers = {authorization: `Basic \x00\x01`};
      const rawReq = createNativeRequest({ headers });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.auth).to.eql({basic: {}});
    });
    it('recognizes basic with no password', function () {
      const username = "hello";
      const headers = {authorization: `Basic ${btoa(username)}`};
      const rawReq = createNativeRequest({ headers });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.auth).to.eql({basic: {username}});
    });
    it('recognizes basic with empty password', function () {
      const username = "hello";
      const headers = {authorization: `Basic ${btoa(`${username}:`)}`};
      const rawReq = createNativeRequest({ headers });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.auth).to.eql({basic: {username, password: ""}});
    });
    it('recognizes basic with password', function () {
      const username = "hello";
      const password = "world";
      const headers = {authorization: `Basic ${btoa(`${username}:${password}`)}`};
      const rawReq = createNativeRequest({ headers });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.auth).to.eql({basic: {username, password}});
    });
    it('recognizes basic with colons in password', function () {
      const username = "hello";
      const password = "w:o:r:l:d";
      const headers = {authorization: `Basic ${btoa(`${username}:${password}`)}`};
      const rawReq = createNativeRequest({ headers });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.auth).to.eql({basic: {username, password}});
    });
  });

  describe('method', function () {
    it('exposes the method of the native request', function () {
      const rawReq = createNativeRequest({
        method: 'potato'
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.method).to.equal('potato');
    });
  });

  describe('originalUrl', function () {
    it('exposes the root relative URL of the native request', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/hi',
        path: '/friend/of/mine',
        search: '?hello=yes&goodbye=indeed'
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.originalUrl).to.equal('/_db/bananas/hi/friend/of/mine?hello=yes&goodbye=indeed');
    });
  });

  describe('url', function () {
    it('exposes the database relative URL of the native request', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/hi',
        path: '/friend/of/mine',
        search: '?hello=yes&goodbye=indeed'
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.url).to.equal('/hi/friend/of/mine?hello=yes&goodbye=indeed');
    });
  });

  describe('path', function () {
    it('exposes the database relative pathname of the native request', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/hi',
        path: '/friend/of/mine',
        search: '?hello=yes&goodbye=indeed'
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.path).to.equal('/hi/friend/of/mine');
    });
  });

  describe('suffix', function () {
    it('exposes the linearized suffix of the native request', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/hi',
        path: '/friend/of/mine',
        search: '?hello=yes&goodbye=indeed'
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.suffix).to.equal('friend/of/mine');
    });
  });

  describe('xhr', function () {
    it('is false if the XHR header was not set', function () {
      const rawReq = createNativeRequest({});
      const req = new SyntheticRequest(rawReq, {});
      expect(req.xhr).to.equal(false);
    });
    it('is true if the XHR header was set correctly', function () {
      const rawReq = createNativeRequest({
        headers: {'x-requested-with': 'XmlHTTPRequest'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.xhr).to.equal(true);
    });
    it('is false if the XHR header was set incorrectly', function () {
      const rawReq = createNativeRequest({
        headers: {'x-requested-with': 'ponies, kisses and rainbows'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.xhr).to.equal(false);
    });
  });

  describe('cookie', function () {
    it('returns nothing if the cookie is not set', function () {
      const rawReq = createNativeRequest({});
      const req = new SyntheticRequest(rawReq, {});
      expect(req.cookie('banana')).to.equal(undefined);
    });
    it('returns the value of the cookie if it is set', function () {
      const rawReq = createNativeRequest({
        cookies: {banana: 'hello'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.cookie('banana')).to.equal('hello');
    });
    it('returns nothing if a secret is provided but no signature exists', function () {
      const rawReq = createNativeRequest({
        cookies: {banana: 'hello'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.cookie('banana', {secret: 'potato'})).to.equal(undefined);
    });
    it('returns nothing if a secret is provided but the signature is invalid', function () {
      const rawReq = createNativeRequest({
        cookies: {
          banana: 'hello',
          'banana.sig': 'invalid'
        }
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.cookie('banana', {secret: 'potato'})).to.equal(undefined);
    });
    it('returns the value if a secret is provided and the signature matches', function () {
      const rawReq = createNativeRequest({
        cookies: {
          banana: 'hello',
          'banana.sig': crypto.hmac('potato', 'hello')
        }
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.cookie('banana', {secret: 'potato'})).to.equal('hello');
    });
    it('accepts a secret instead of an options object', function () {
      const rawReq = createNativeRequest({
        cookies: {
          banana: 'hello',
          'banana.sig': crypto.hmac('potato', 'hello')
        }
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.cookie('banana', 'potato')).to.equal('hello');
    });
    it('correctly handles other algorithms', function () {
      const rawReq = createNativeRequest({
        cookies: {
          banana: 'hello',
          'banana.sig': crypto.hmac('potato', 'hello', 'sha512')
        }
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.cookie('banana', {
        secret: 'potato',
        algorithm: 'sha512'
      })).to.equal('hello');
    });
    it('correctly handles algorithm mismatches', function () {
      const rawReq = createNativeRequest({
        cookies: {
          banana: 'hello',
          'banana.sig': crypto.hmac('potato', 'hello', 'sha256')
        }
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.cookie('banana', {
        secret: 'potato',
        algorithm: 'sha512'
      })).to.equal(undefined);
    });
  });

  describe('makeAbsolute', function () {
    it('correctly generates absolute URLs', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/foxx',
        headers: {host: 'www.example.com:9999'},
        path: '/some/place/special',
        search: '?a=1&b=2',
        protocol: 'https'
      });
      const req = new SyntheticRequest(rawReq, {mount: '/foxx'});
      expect(req.makeAbsolute('/another/place', 'x=y&z=w')).to.equal(
        'https://www.example.com:9999/_db/bananas/foxx/another/place?x=y&z=w'
      );
    });
    it('also accepts a query params object', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/foxx',
        headers: {host: 'www.example.com:9999'},
        path: '/some/place/special',
        search: '?a=1&b=2',
        protocol: 'https'
      });
      const req = new SyntheticRequest(rawReq, {mount: '/foxx'});
      expect(req.makeAbsolute('/another/place', {x: 'y', z: 'w'})).to.equal(
        'https://www.example.com:9999/_db/bananas/foxx/another/place?x=y&z=w'
      );
    });
    it('omits the port if port is 80 and protocol is http', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/foxx',
        headers: {host: 'www.example.com:80'},
        path: '/some/place/special',
        search: '?a=1&b=2',
        protocol: 'http'
      });
      const req = new SyntheticRequest(rawReq, {mount: '/foxx'});
      expect(req.makeAbsolute('/another/place', 'x=y&z=w')).to.equal(
        'http://www.example.com/_db/bananas/foxx/another/place?x=y&z=w'
      );
    });
    it('omits the port if port is 443 and protocol is https', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/foxx',
        headers: {host: 'www.example.com:443'},
        path: '/some/place/special',
        search: '?a=1&b=2',
        protocol: 'https'
      });
      const req = new SyntheticRequest(rawReq, {mount: '/foxx'});
      expect(req.makeAbsolute('/another/place', 'x=y&z=w')).to.equal(
        'https://www.example.com/_db/bananas/foxx/another/place?x=y&z=w'
      );
    });
    it('works without query params', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/foxx',
        headers: {host: 'www.example.com:9999'},
        path: '/some/place/special',
        search: '?a=1&b=2',
        protocol: 'https'
      });
      const req = new SyntheticRequest(rawReq, {mount: '/foxx'});
      expect(req.makeAbsolute('/another/place')).to.equal(
        'https://www.example.com:9999/_db/bananas/foxx/another/place'
      );
    });
    it('correctly handles non-root paths', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/foxx',
        headers: {host: 'www.example.com:9999'},
        path: '/some/place/special',
        search: '?a=1&b=2',
        protocol: 'https'
      });
      const req = new SyntheticRequest(rawReq, {mount: '/foxx'});
      expect(req.makeAbsolute('another/place')).to.equal(
        'https://www.example.com:9999/_db/bananas/foxx/another/place'
      );
    });
    it('correctly handles relative paths', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/foxx',
        headers: {host: 'www.example.com:9999'},
        path: '/some/place/special',
        search: '?a=1&b=2',
        protocol: 'https'
      });
      const req = new SyntheticRequest(rawReq, {mount: '/foxx'});
      expect(req.makeAbsolute('../another/place')).to.equal(
        'https://www.example.com:9999/_db/bananas/another/place'
      );
    });
    it('correctly handles unix sockets', function () {
      const socketPath = require('internal').platform.substr(0, 3) === 'win'
        ? "C:\\tmp\\arangod.sock"
        : "/tmp/arangod.sock";
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/foxx',
        path: '/some/place/special',
        search: '?a=1&b=2',
        protocol: 'https',
        socketPath
      });
      const req = new SyntheticRequest(rawReq, {mount: '/foxx'});
      expect(req.makeAbsolute('../another/place')).to.equal(
        `https://unix:${socketPath}:/_db/bananas/another/place`
      );
    });
    it('correctly handles relative unix sockets', function () {
      const rawReq = createNativeRequest({
        db: 'bananas',
        mount: '/foxx',
        path: '/some/place/special',
        search: '?a=1&b=2',
        protocol: 'https',
        socketPath: 'arangod.sock'
      });
      const absPath = resolvePath('arangod.sock');
      const req = new SyntheticRequest(rawReq, {mount: '/foxx'});
      expect(req.makeAbsolute('../another/place')).to.equal(
        `https://unix:${absPath}:/_db/bananas/another/place`
      );
    });
  });

  describe('accepts', function () {
    it('returns the acceptable type', function () {
      const rawReq = createNativeRequest({
        headers: {accept: 'application/json, text/javascript; q=0.01'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.accepts(['text/javascript'])).to.equal('text/javascript');
    });
    it('returns the preferred acceptable type', function () {
      const rawReq = createNativeRequest({
        headers: {accept: 'application/json, text/javascript; q=0.01'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.accepts(['text/javascript', 'json'])).to.equal('json');
    });
    it('returns false if no type is acceptable', function () {
      const rawReq = createNativeRequest({
        headers: {accept: 'application/json, text/javascript; q=0.01'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.accepts(['kitty/soft', 'kitty/warm'])).to.equal(false);
    });
    it('accepts multiple arguments instead of an array', function () {
      const rawReq = createNativeRequest({
        headers: {accept: 'application/json, text/javascript; q=0.01'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.accepts('potato', 'json')).to.equal('json');
    });
  });

  describe('acceptsCharsets', function () {
    it('returns the acceptable charset', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-charset': 'utf-8, iso-8859-1; q=0.01'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsCharsets(['iso-8859-1'])).to.equal('iso-8859-1');
    });
    it('returns the preferred acceptable charset', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-charset': 'utf-8, iso-8859-1; q=0.01'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsCharsets(['iso-8859-1', 'utf-8'])).to.equal('utf-8');
    });
    it('returns false if no charset is acceptable', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-charset': 'utf-8, iso-8859-1; q=0.01'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsCharsets(['kitty-soft', 'kitty-warm'])).to.equal(false);
    });
    it('accepts multiple arguments instead of an array', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-charset': 'utf-8, iso-8859-1; q=0.01'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsCharsets('potato', 'utf-8')).to.equal('utf-8');
    });
  });

  describe('acceptsEncodings', function () {
    it('returns the acceptable encoding', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-encoding': 'gzip, deflate, sdch'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsEncodings(['deflate'])).to.equal('deflate');
    });
    it('returns the preferred acceptable encoding', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-encoding': 'gzip, deflate, sdch'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsEncodings(['deflate', 'gzip'])).to.equal('gzip');
    });
    it('returns false if no encoding is acceptable', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-encoding': 'gzip, deflate, sdch'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsEncodings(['softkitty', 'warmkitty'])).to.equal(false);
    });
    it('accepts multiple arguments instead of an array', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-encoding': 'gzip, deflate, sdch'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsEncodings('potato', 'gzip')).to.equal('gzip');
    });
  });

  describe('acceptsLanguages', function () {
    it('returns the acceptable language', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-language': 'en-GB,en;q=0.8,en-US;q=0.6,de;q=0.4,de-DE;q=0.2'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsLanguages(['de-DE'])).to.equal('de-DE');
    });
    it('returns the preferred acceptable language', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-language': 'en-GB,en;q=0.8,en-US;q=0.6,de;q=0.4,de-DE;q=0.2'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsLanguages(['de-DE', 'en-GB'])).to.equal('en-GB');
    });
    it('returns false if no language is acceptable', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-language': 'en-GB,en;q=0.8,en-US;q=0.6,de;q=0.4,de-DE;q=0.2'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsLanguages(['kitty-SOFT', 'kitty-WARM'])).to.equal(false);
    });
    it('accepts multiple arguments instead of an array', function () {
      const rawReq = createNativeRequest({
        headers: {'accept-language': 'en-GB,en;q=0.8,en-US;q=0.6,de;q=0.4,de-DE;q=0.2'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.acceptsLanguages('potato', 'en-GB')).to.equal('en-GB');
    });
  });

  describe('range', function () {
    it('returns -2 for malformed header', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'banana'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.range(Infinity)).to.equal(-2);
    });
    it('returns -1 for invalid header range', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'bananas=500-20'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.range(Infinity)).to.equal(-1);
    });
    it('parses single ranges', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'bananas=0-499'}
      });
      const req = new SyntheticRequest(rawReq, {});
      const range = req.range(Infinity);
      expect(range).to.have.a.property('type', 'bananas');
      expect(range).to.have.a.property('length', 1);
      expect(range[0]).to.eql({start: 0, end: 499});
    });
    it('parses multiple ranges', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'bananas=0-499,500-749'}
      });
      const req = new SyntheticRequest(rawReq, {});
      const range = req.range(Infinity);
      expect(range).to.have.a.property('type', 'bananas');
      expect(range).to.have.a.property('length', 2);
      expect(range[0]).to.eql({start: 0, end: 499});
      expect(range[1]).to.eql({start: 500, end: 749});
    });
    it('caps end at size', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'bananas=0-999'}
      });
      const req = new SyntheticRequest(rawReq, {});
      const range = req.range(100);
      expect(range).to.have.a.property('type', 'bananas');
      expect(range).to.have.a.property('length', 1);
      expect(range[0]).to.eql({start: 0, end: 99});
    });
    it('parses "-X"', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'bananas=-400'}
      });
      const req = new SyntheticRequest(rawReq, {});
      const range = req.range(1000);
      expect(range).to.have.a.property('type', 'bananas');
      expect(range).to.have.a.property('length', 1);
      expect(range[0]).to.eql({start: 600, end: 999});
    });
    it('parses "X-"', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'bananas=400-'}
      });
      const req = new SyntheticRequest(rawReq, {});
      const range = req.range(1000);
      expect(range).to.have.a.property('type', 'bananas');
      expect(range).to.have.a.property('length', 1);
      expect(range[0]).to.eql({start: 400, end: 999});
    });
    it('parses "0-"', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'bananas=0-'}
      });
      const req = new SyntheticRequest(rawReq, {});
      const range = req.range(1000);
      expect(range).to.have.a.property('type', 'bananas');
      expect(range).to.have.a.property('length', 1);
      expect(range[0]).to.eql({start: 0, end: 999});
    });
    it('parses "-1"', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'bananas=-1'}
      });
      const req = new SyntheticRequest(rawReq, {});
      const range = req.range(1000);
      expect(range).to.have.a.property('type', 'bananas');
      expect(range).to.have.a.property('length', 1);
      expect(range[0]).to.eql({start: 999, end: 999});
    });
    it('parses "0-0"', function () {
      const rawReq = createNativeRequest({
        headers: {range: 'bananas=0-0'}
      });
      const req = new SyntheticRequest(rawReq, {});
      const range = req.range(Infinity);
      expect(range).to.have.a.property('type', 'bananas');
      expect(range).to.have.a.property('length', 1);
      expect(range[0]).to.eql({start: 0, end: 0});
    });
  });

  describe('is', function () {
    it('returns false if no content-type is set', function () {
      const rawReq = createNativeRequest({});
      const req = new SyntheticRequest(rawReq, {});
      expect(req.is(['application/xhtml+xml', 'text/html'])).to.equal(false);
    });
    it('returns the first matching mime type', function () {
      const rawReq = createNativeRequest({
        headers: {'content-type': 'text/html; charset=utf-8'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.is(['application/xhtml+xml', 'text/html'])).to.equal('text/html');
    });
    it('returns false if no mime type matches', function () {
      const rawReq = createNativeRequest({
        headers: {'content-type': 'text/html; charset=utf-8'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.is(['application/xhtml+xml'])).to.equal(false);
    });
    it('accepts multiple arguments instead of an array', function () {
      const rawReq = createNativeRequest({
        headers: {'content-type': 'text/html; charset=utf-8'}
      });
      const req = new SyntheticRequest(rawReq, {});
      expect(req.is('application/xhtml+xml', 'text/html')).to.equal('text/html');
    });
  });
});

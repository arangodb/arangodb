'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const parseUrl = require('url').parse;
const {
  posix: {join: joinPath},
  resolve: resolvePath
} = require('path');
const typeIs = require('type-is').is;
const accepts = require('accepts');
const parseRange = require('range-parser');
const querystring = require('querystring');
const getTrustedProxies = require('internal').trustedProxies;
const crypto = require('@arangodb/crypto');
const Netmask = require('netmask').Netmask;
let trustedProxyBlocks;

function generateTrustedProxyBlocks () {
  const trustedProxies = getTrustedProxies();
  if (!Array.isArray(trustedProxies)) {
    return false;
  }
  const blocks = [];
  for (const trustedProxy of trustedProxies) {
    try {
      blocks.push(new Netmask(trustedProxy));
    } catch (e) {
      console.warnLines(
        `Error parsing trusted proxy "${trustedProxy}":\n${e.stack}`
      );
    }
  }
  return blocks;
}

function shouldTrustProxy (address) {
  if (trustedProxyBlocks === undefined) {
    trustedProxyBlocks = generateTrustedProxyBlocks();
  }
  return !trustedProxyBlocks || trustedProxyBlocks
      .some((block) => block.contains(address));
}

module.exports =
  class SyntheticRequest {
    constructor (req, context) {
      this._url = parseUrl(req.url);
      this._raw = req;
      this.context = context;
      this.suffix = req.suffix.join('/');
      this.baseUrl = joinPath('/_db', encodeURIComponent(this._raw.database));
      this.path = this._url.pathname;
      this.pathParams = {};
      this.queryParams = querystring.parse(this._url.query);
      if (req.hasOwnProperty('rawRequestBody')) {
        this.body = req.rawRequestBody;
      }
      else {
        this.body = req.requestBody;
      }
      this.rawBody = this.body;

      this.trustProxy = (
        typeof context.trustProxy === 'boolean'
          ? context.trustProxy
          : shouldTrustProxy(req.client.address)
      );

      const server = extractServer(req, this.trustProxy);
      this.protocol = server.protocol;
      this.hostname = server.hostname;
      this.port = server.port;

      const client = extractClient(req, this.trustProxy);
      this.remoteAddress = client.ip;
      this.remoteAddresses = client.ips;
      this.remotePort = client.port;
    }

    _PRINT (ctx) {
      ctx.output += '[IncomingRequest]';
    }

    // Node compat

    get headers () {
      return this._raw.headers;
    }

    set headers (headers) {
      this._raw.headers = headers;
    }

    get method () {
      return this._raw.requestType;
    }

    // Express compat

    get originalUrl () {
      return joinPath(
          '/_db',
          encodeURIComponent(this._raw.database),
          this._url.pathname
        ) + (this._url.search || '');
    }

    get secure () {
      return this.protocol === 'https';
    }

    get url () {
      return this.path + (this._url.search || '');
    }

    get xhr () {
      const header = this.headers['x-requested-with'];
      return Boolean(header && header.toLowerCase() === 'xmlhttprequest');
    }

    accepts () {
      const accept = accepts(this);
      return accept.types.apply(accept, arguments);
    }

    acceptsCharsets () {
      const accept = accepts(this);
      return accept.charsets.apply(accept, arguments);
    }

    acceptsEncodings () {
      const accept = accepts(this);
      return accept.encodings.apply(accept, arguments);
    }

    acceptsLanguages () {
      const accept = accepts(this);
      return accept.languages.apply(accept, arguments);
    }

    range (size) {
      const range = this.headers.range;
      if (!range) {
        return undefined;
      }
      return parseRange((size || size === 0) ? size : Infinity, range);
    }

    get (name) {
      const lc = name.toLowerCase();
      if (lc === 'referer' || lc === 'referrer') {
        return this.headers.referer || this.headers.referrer;
      }
      return this.headers[lc];
    }

    header (name) {
      return this.get(name);
    }

    is (mediaType) {
      if (!this.headers['content-type']) {
        return false;
      }
      if (!Array.isArray(mediaType)) {
        mediaType = Array.prototype.slice.call(arguments);
      }
      return typeIs(this, mediaType);
    }

    // idiosyncratic
    get authorized () {
      return this._raw.authorized;
    }

    get arangoUser () {
      if (this._raw.authorized) {
        return this._raw.user;
      } else {
        return null;
      }
    }

    get arangoVersion () {
      return this._raw.compatibility;
    }

    get auth () {
      const header = this.get("authorization") || "";
      let match = header.match(/^Bearer (.*)$/);
      if (match) {
        return {bearer: match[1]};
      }
      match = header.match(/^Basic (.*)$/);
      if (match) {
        let credentials = "";
        try {
          credentials = new Buffer(match[1], "base64").toString("utf-8");
        } catch (e) {}
        if (!credentials) return {basic: {}};
        const i = credentials.indexOf(":");
        if (i === -1) {
          return {basic: {username: credentials}};
        }
        return {
          basic: {
            username: credentials.slice(0, i),
            password: credentials.slice(i + 1)
          }
        };
      }
      return null;
    }

    get database () {
      return this._raw.database;
    }

    json () {
      if (!this.rawBody || !this.rawBody.length) {
        return undefined;
      }
      return JSON.parse(this.rawBody.toString('utf-8'));
    }

    param (name) {
      if (hasOwnProperty.call(this.pathParams, name)) {
        return this.pathParams[name];
      }
      return this.queryParams[name];
    }

    cookie (name, opts) {
      if (typeof opts === 'string') {
        opts = {secret: opts};
      } else if (!opts) {
        opts = {};
      }
      const value = this._raw.cookies[name];
      if (value && opts.secret) {
        const signature = this._raw.cookies[`${name}.sig`];
        const valid = crypto.constantEquals(
          signature || '',
          crypto.hmac(opts.secret, value, opts.algorithm)
        );
        if (!valid) {
          return undefined;
        }
      }
      return value;
    }

    makeAbsolute (path, query) {
      const pathname = joinPath(
        '/_db',
        encodeURIComponent(this._raw.database),
        this.context.mount,
        path
      );
      let search = "";
      if (query) {
        if (typeof query === "string") {
          search = query.charAt(0) === "?" ? query : `?${query}`;
        } else {
          search = `?${querystring.stringify(query)}`;
        }
      }
      if (this._raw.portType === "unix") {
        const rawSocketPath = this._raw.server.endpoint.slice("unix://".length);
        const socketPath = resolvePath(rawSocketPath);
        return `${this.protocol}://unix:${socketPath}:${pathname}${search}`;
      }
      const port = (this.secure ? this.port !== 443 : this.port !== 80) && this.port;
      const host = port ? `${this.hostname}:${this.port}` : this.hostname;
      return `${this.protocol}://${host}${pathname}${search}`;
    }
};

function extractServer (req, trustProxy) {
  let hostname = req.server.address;
  let port = req.server.port;
  const protocol = (
  (trustProxy && req.headers['x-forwarded-proto'])
  || req.protocol
  );
  const secure = protocol === 'https';
  const hostHeader = (
  (trustProxy && req.headers['x-forwarded-host'])
  || req.headers.host
  );
  if (hostHeader) {
    const match = hostHeader.match(/^(.*):(\d+)$/) || [hostHeader, hostHeader];
    if (match) {
      hostname = match[1];
      port = match[2] ? Number(match[2]) : secure ? 443 : 80;
    }
  }
  return {protocol, hostname, port};
}

function extractClient (req, trustProxy) {
  let ip = req.client.address;
  let ips = [ip];
  const port = Number(
    (trustProxy && req.headers['x-forwarded-port'])
    || req.client.port
  );
  const forwardedFor = req.headers['x-forwarded-for'];
  if (trustProxy && forwardedFor) {
    const tokens = forwardedFor.split(/\s*,\s*/g).filter(Boolean);
    if (tokens.length) {
      ips = tokens;
      ip = tokens[0];
    }
  }
  return {ips, ip, port};
}

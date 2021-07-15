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

const _ = require('lodash');
const fs = require('fs');
const vary = require('vary');
const httperr = require('http-errors');
const statuses = require('statuses');
const mediaTyper = require('media-typer');
const mimeTypes = require('mime-types');
const typeIs = require('type-is');
const contentDisposition = require('content-disposition');
const actions = require('@arangodb/actions');
const crypto = require('@arangodb/crypto');

const MIME_BINARY = 'application/octet-stream';
const MIME_JSON = 'application/json; charset=utf-8';

module.exports =
  class SyntheticResponse {
    constructor (res, context) {
      this._raw = res;
      this._responses = new Map();
      this.context = context;
    }

    _PRINT (ctx) {
      ctx.output += '[OutgoingResponse]';
    }

    // Node compat

    get headers () {
      if (!this._raw.headers) {
        this._raw.headers = {};
      }
      return this._raw.headers;
    }

    set headers (headers) {
      this._raw.headers = headers;
      if (!headers) {
        return;
      }
      for (const name of Object.keys(headers)) {
        if (name.toLowerCase() === 'content-type') {
          this._raw.contentType = headers[name];
        }
      }
    }

    get statusCode () {
      return this._raw.responseCode;
    }

    set statusCode (value) {
      this._raw.responseCode = value;
    }

    get body () {
      return this._raw.body;
    }

    set body (data) {
      if (data === null || data === undefined) {
        delete this._raw.body;
        return;
      }
      if (typeof data === 'string' || data instanceof Buffer) {
        this._raw.body = data;
      } else if (typeof data === 'object') {
        if (this.context.isDevelopment) {
          this._raw.body = JSON.stringify(data, null, 2);
        } else {
          this._raw.body = JSON.stringify(data);
        }
      } else {
        this._raw.body = String(data);
      }
    }

    getHeader (name) {
      name = name.toLowerCase();
      if (name === 'content-type') {
        return this._raw.contentType;
      }
      return this.headers[name];
    }

    removeHeader (name) {
      name = name.toLowerCase();
      if (name === 'content-type') {
        delete this._raw.contentType;
      }
      delete this.headers[name];
      return this;
    }

    setHeader (name, value) {
      if (!name) {
        return this;
      }
      name = name.toLowerCase();
      if (name === 'content-type') {
        this._raw.contentType = value;
      } else {
        this.headers[name] = value;
      }
      return this;
    }

    write (data) {
      const bodyIsBuffer = this._raw.body instanceof Buffer;
      const dataIsBuffer = data instanceof Buffer;
      if (data === null || data === undefined) {
        return this;
      }
      if (!dataIsBuffer) {
        if (typeof data === 'object') {
          if (this.context.isDevelopment) {
            data = JSON.stringify(data, null, 2);
          } else {
            data = JSON.stringify(data);
          }
        } else {
          data = String(data);
        }
      }
      if (!this._raw.body) {
        this._raw.body = data;
      } else if (bodyIsBuffer || dataIsBuffer) {
        this._raw.body = Buffer.concat([
          bodyIsBuffer ? this._raw.body : new Buffer(this._raw.body),
          dataIsBuffer ? data : new Buffer(data)
        ]);
      } else {
        this._raw.body += data;
      }
      return this;
    }

    // Express compat

    attachment (filename) {
      this.headers['content-disposition'] = contentDisposition(filename);
      if (filename && !this._raw.contentType) {
        this._raw.contentType = mimeTypes.lookup(filename) || MIME_BINARY;
      }
      return this;
    }

    download (path, filename) {
      this.attachment(filename || path);
      this.sendFile(path);
      return this;
    }

    json (value, pretty) {
      if (!this._raw.contentType) {
        this._raw.contentType = MIME_JSON;
      }
      if (pretty || this.context.isDevelopment) {
        this._raw.body = JSON.stringify(value, null, 2);
      } else {
        this._raw.body = JSON.stringify(value);
      }
      return this;
    }

    redirect (status, path) {
      if (!path) {
        path = status;
        status = undefined;
      }
      if (status === 'permanent') {
        status = 301;
      }
      if (status || !this.statusCode) {
        this.statusCode = status || 302;
      }
      this.setHeader('location', path);
      return this;
    }

    sendFile (filename, opts) {
      if (typeof opts === 'boolean') {
        opts = {lastModified: opts};
      }
      if (!opts) {
        opts = {};
      }
      this._raw.body = fs.readFileSync(filename);
      if (opts.lastModified || (
        opts.lastModified !== false && !this.headers['last-modified']
        )) {
        const lastModified = new Date(fs.mtime(filename) * 1000);
        this.headers['last-modified'] = lastModified.toUTCString();
      }
      if (!this._raw.contentType) {
        this._raw.contentType = mimeTypes.lookup(filename) || MIME_BINARY;
      }
      return this;
    }

    sendStatus (status) {
      if (typeof status === 'string') {
        status = statuses(status);
      }
      const message = String(statuses[status] || status);
      this.statusCode = status;
      this.body = message;
      return this;
    }

    set (name, value) {
      if (name && typeof name === 'object') {
        _.each(name, (v, k) => this.set(k, v));
      } else {
        this.setHeader(name, value);
      }
      return this;
    }

    status (status) {
      if (typeof status === 'string') {
        status = statuses(status);
      }
      this.statusCode = status;
      return this;
    }

    vary (values) {
      let header = this.getHeader('vary') || '';
      if (!Array.isArray(values)) {
        values = Array.prototype.slice.call(arguments);
      }
      for (const value of values) {
        header = vary.append(header, value);
      }
      this.setHeader('vary', header);
      return this;
    }

    // idiosyncratic

    cookie (name, value, opts) {
      if (!opts) {
        opts = {};
      } else if (typeof opts === 'string') {
        opts = {secret: opts};
      } else if (typeof opts === 'number') {
        opts = {ttl: opts};
      }
      const ttl = (
      (typeof opts.ttl === 'number' && opts.ttl !== Infinity)
        ? opts.ttl
        : undefined
      );
      actions.addCookie(
        this._raw,
        name,
        value,
        ttl,
        opts.path,
        opts.domain,
        opts.secure,
        opts.httpOnly
      );
      if (opts.secret) {
        const signature = crypto.hmac(opts.secret, value, opts.algorithm);
        actions.addCookie(
          this._raw,
          `${name}.sig`,
          signature,
          ttl,
          opts.path,
          opts.domain,
          opts.secure,
          opts.httpOnly
        );
      }
      return this;
    }

    throw (status, reason, options) {
      if (typeof status !== "number") {
        status = statuses(status);
      }
      if (reason) {
        if (reason instanceof Error) {
          const err = reason;
          reason = err.message;
          options = Object.assign(
            {
              cause: err,
              errorNum: err.errorNum
            },
            options
          );
        } else if (typeof reason === "object") {
          options = reason;
          reason = undefined;
        } else if (typeof reason !== "string") {
          reason = String(reason);
        }
      }
      throw Object.assign(
        httperr(status, reason),
        {statusCode: status, status},
        options
      );
    }

    send (body, type) {
      if (body && body.isArangoResultSet) {
        body = body.toArray();
      }

      if (!type) {
        type = 'auto';
      }

      let contentType;
      const status = this.statusCode || 200;
      const response = this._responses.get(status);

      if (response) {
        if (response.model && response.model.forClient) {
          if (response.multiple && Array.isArray(body)) {
            body = body.map((item) => response.model.forClient(item));
          } else {
            body = response.model.forClient(body);
          }
        }
        if (type === 'auto' && response.contentTypes) {
          type = response.contentTypes[0];
          contentType = type;
        }
      }

      if (type === 'auto') {
        if (body instanceof Buffer) {
          type = MIME_BINARY;
        } else if (body && typeof body === 'object') {
          type = 'json';
        } else {
          type = 'html';
        }
      }

      type = mimeTypes.lookup(type) || type;

      let handler;
      for (const entry of this.context.service.types.entries()) {
        const key = entry[0];
        const value = entry[1];
        let match;
        if (key instanceof RegExp) {
          match = type.test(key);
        } else if (typeof key === 'function') {
          match = key(type);
        } else {
          match = typeIs.is(key, type);
        }
        if (match && value.forClient) {
          contentType = key;
          handler = value;
          break;
        }
      }

      if (handler) {
        const result = handler.forClient(body, this, mediaTyper.parse(contentType));
        if (result.headers || result.data) {
          contentType = result.headers['content-type'] || contentType;
          this.set(result.headers);
          body = result.data;
        } else {
          body = result;
        }
      }

      this._raw.body = body;
      if (contentType) {
        this._raw.contentType = contentType;
      }

      return this;
    }

    type (type) {
      if (type) {
        this._raw.contentType = mimeTypes.lookup(type) || type;
      }
      return this._raw.contentType;
    }
};

'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Router Synthetic Response
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2016 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');
const vary = require('vary');
const statuses = require('statuses');
const contentDisposition = require('content-disposition');
const guessContentType = require('@arangodb').guessContentType;
const addCookie = require('@arangodb/actions').addCookie;
const crypto = require('@arangodb/crypto');


module.exports = class SyntheticResponse {
  constructor(res) {
    this._raw = res;
  }

  // Node compat

  get headers() {
    return this._raw.headers;
  }

  get statusCode() {
    return this._raw.responseCode;
  }

  set statusCode(value) {
    this._raw.responseCode = value;
  }

  getHeader(name) {
    name = name.toLowerCase();
    if (name === 'content-type') {
      return this._raw.contentType;
    }
    return this.headers[name];
  }

  removeHeader(name) {
    name = name.toLowerCase();
    if (name === 'content-type') {
      delete this._raw.contentType;
    }
    delete this.headers[name];
    return this;
  }

  setHeader(name, value) {
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

  write(body) {
    const bodyIsBuffer = body && body instanceof Buffer;
    if (!body) {
      body = '';
    } else if (!bodyIsBuffer) {
      if (typeof body === 'object') {
        body = JSON.stringify(body);
      } else {
        body = String(body);
      }
    }
    if (!this._raw.body) {
      this._raw.body = body;
    } else if (this._raw.body instanceof Buffer) {
      this._raw.body = Buffer.concat(
        this._raw.body,
        bodyIsBuffer ? body : new Buffer(body)
      );
    } else {
      this._raw.body += body;
    }
    return this;
  }

  // Express compat

  attachment(filename) {
    this.headers['content-disposition'] = contentDisposition(filename);
    if (filename && !this._raw.contentType) {
      this._raw.contentType = guessContentType(filename);
    }
    return this;
  }

  download(path, filename) {
    this.attachment(filename || path);
    this.sendFile(path);
    return this;
  }

  json(value) {
    if (!this._raw.contentType) {
      this._raw.contentType = 'application/json';
    }
    this._raw.body = JSON.stringify(value);
    return this;
  }

  redirect(status, path) {
    if (!path) {
      path = status;
      status = undefined;
    }
    if (status === 'permanent') {
      status = 301;
    }
    if (status || !this._raw.responseCode) {
      this._raw.responseCode = status || 302;
    }
    this.setHeader('location', path);
    return this;
  }

  send(body) {
    if (!body) {
      body = '';
    }
    let impliedType = 'text/html';
    if (body instanceof Buffer) {
      impliedType = 'application/octet-stream';
    } else if (body && typeof body === 'object') {
      body = JSON.stringify(body);
      impliedType = 'application/json';
    } else if (typeof body !== 'string') {
      body = String(body);
    }
    if (impliedType && !this._raw.contentType) {
      this._raw.contentType = impliedType;
    }
    this._raw.body = body;
    return this;
  }

  sendFile(filename, opts) {
    if (!opts) {
      opts = {};
    }
    this._raw.body = fs.readBuffer(filename);
    if (opts.lastModified !== false && !this.headers['last-modified']) {
      const lastModified = new Date(fs.mtime(filename) * 1000);
      this.headers['last-modified'] = lastModified.toUTCString();
    }
    if (!this._raw.contentType) {
      this._raw.contentType = guessContentType(filename, 'application/octet-stream');
    }
    return this;
  }

  sendStatus(status) {
    const message = (
      (Number(status) !== 306 && statuses[status])
      || String(status)
    );
    this._raw.responseCode = status;
    this.send(message);
    return this;
  }

  set(name, value) {
    if (name && typeof name === 'object') {
      _.each(name, function (key) {
        this.set(key, name[key]);
      }, this);
    } else {
      this.setHeader(name, value);
    }
    return this;
  }

  status(statusCode) {
    this._raw.responseCode = statusCode;
    return this;
  }

  vary(value) {
    const header = this.getHeader('vary');
    if (header) {
      value = vary.append(header, value);
    }
    this.setHeader('vary', value);
    return this;
  }

  // idiosyncratic

  cookie(name, value, opts) {
    if (!opts) {
      opts = {};
    } else if (typeof opts === 'string') {
      opts = {secret: opts};
    } else if (typeof opts === 'number') {
      opts = {ttl: opts};
    }
    const ttl = (
      (typeof opts.ttl === 'number' && opts.ttl !== Infinity && opts.ttl)
      || undefined
    );
    addCookie(
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
      addCookie(
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
};

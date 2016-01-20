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
const mediaTyper = require('media-typer');
const mimeTypes = require('mime-types');
const typeIs = require('type-is');
const contentDisposition = require('content-disposition');
const guessContentType = require('@arangodb').guessContentType;
const addCookie = require('@arangodb/actions').addCookie;
const crypto = require('@arangodb/crypto');

const MIME_BINARY = 'application/octet-stream';
const MIME_JSON = 'application/json; charset=utf-8';


module.exports = class SyntheticResponse {
  constructor(res, context) {
    this._raw = res;
    this._responses = new Map();
    this.context = context;
  }

  // Node compat

  get headers() {
    if (!this._raw.headers) {
      this._raw.headers = {};
    }
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

  write(data) {
    const bodyIsBuffer = this.body instanceof Buffer;
    const dataIsBuffer = data instanceof Buffer;
    if (!data) {
      data = '';
    } else if (!dataIsBuffer) {
      if (typeof data === 'object') {
        data = JSON.stringify(data);
      } else {
        data = String(data);
      }
    }
    if (!this.body) {
      this._raw.body = data;
    } else if (bodyIsBuffer || dataIsBuffer) {
      this._raw.body = Buffer.concat(
        bodyIsBuffer ? this.body : new Buffer(this.body),
        dataIsBuffer ? data : new Buffer(data)
      );
    } else {
      this.body += data;
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
      this._raw.contentType = MIME_JSON;
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
    if (status || !this.statusCode) {
      this.statusCode = status || 302;
    }
    this.setHeader('location', path);
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
      this._raw.contentType = guessContentType(filename, MIME_BINARY);
    }
    return this;
  }

  sendStatus(status) {
    const message = (
      (Number(status) !== 306 && statuses[status])
      || String(status)
    );
    this.statusCode = status;
    this.send(message);
    return this;
  }

  set(name, value) {
    if (name && typeof name === 'object') {
      _.each(name, function (v, k) {
        this.set(k, v);
      }, this);
    } else {
      this.setHeader(name, value);
    }
    return this;
  }

  status(statusCode) {
    this.statusCode = statusCode;
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

  send(body, type) {
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
          body = body.map(function (item) {
            return response.model.forClient(item);
          });
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
    this._raw.contentType = contentType || this._raw.contentType;

    return this;
  }
};

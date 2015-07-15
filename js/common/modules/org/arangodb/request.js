/*jshint globalstrict: true, sub: true */
/*global exports: true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief node-request-style HTTP requests
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
/// @author Alan Plum
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var Buffer = require('buffer').Buffer;
var extend = require('underscore').extend;
var httperr = require('http-errors');
var is = require('org/arangodb/is');
var querystring = require('querystring');
var qs = require('qs');
var url = require('url');

class Response {
  throw(msg) {
    var err = new httperr[this.status](msg || this.message);
    err.details = this;
    throw err;
  }
  constructor(res, encoding, json) {
    this.status = this.statusCode = res.code;
    this.message = res.message;
    this.headers = res.headers ? res.headers : {};
    this.body = this.rawBody = res.body;
    if (this.body && encoding !== null) {
      this.body = this.body.toString(encoding || 'utf-8');
      if (json) {
        try {
          this.body = JSON.parse(this.body);
        } catch (e) {
          // Do nothing.
        }
      }
    }
  }
}

function querystringify(query, useQuerystring) {
  if (!query) {
    return '';
  }
  if (typeof query === 'string') {
    return query.charAt(0) === '?' ? query.slice(1) : query;
  }
  return (useQuerystring ? querystring : qs).stringify(query)
  .replace(/[!'()*]/g, function(c) {
    // Stricter RFC 3986 compliance
    return '%' + c.charCodeAt(0).toString(16);
  });
}

function request(req) {
  if (typeof req === 'string') {
    req = {url: req, method: 'GET'};
  }

  var path = req.url || req.uri;
  if (!path) {
    throw new Error('Request URL must not be empty.');
  }

  var pathObj = typeof path === 'string' ? url.parse(path) : path;
  if (pathObj.auth) {
    var auth = pathObj.auth.split(':');
    req = extend({
      auth: {
        username: decodeURIComponent(auth[0]),
        password: decodeURIComponent(auth[1])
      }
    }, req);
    delete pathObj.auth;
  }
  var query = typeof req.qs === 'string' ? req.qs : querystringify(req.qs, req.useQuerystring);
  if (query) {
    pathObj.search = query;
  }
  path = url.format(pathObj);

  var contentType;
  var body = req.body;
  if (req.json) {
    body = JSON.stringify(body);
    contentType = 'application/json';
  } else if (typeof body === 'string') {
    contentType = 'text/plain; charset=utf-8';
  } else if (typeof body === 'object' && body instanceof Buffer) {
    contentType = 'application/octet-stream';
  } else if (!body) {
    if (req.form) {
      contentType = 'application/x-www-form-urlencoded';
      body = typeof req.form === 'string' ? req.form : querystringify(req.form, req.useQuerystring);
    } else if (req.formData) {
      // contentType = 'multipart/form-data';
      // body = formData(req.formData);
      throw new Error('Multipart form encoding is currently not supported.');
    } else if (req.multipart) {
      // contentType = 'multipart/related';
      // body = multipart(req.multipart);
      throw new Error('Multipart encoding is currently not supported.');
    }
  }

  var headers = {'content-type': contentType};

  if (req.headers) {
    Object.keys(req.headers).forEach(function (name) {
      headers[name.toLowerCase()] = req.headers[name];
    });
  }

  if (req.auth) {
    headers['authorization'] = (
      req.auth.bearer ?
      'Bearer ' + req.auth.bearer :
      'Basic ' + new Buffer(
        req.auth.username + ':' +
        req.auth.password
      ).toString('base64')
    );
  }

  var options = {
    method: (req.method || 'get').toUpperCase(),
    headers: headers,
    returnBodyAsBuffer: true
  };
  if (is.existy(req.timeout)) {
    options.timeout = req.timeout;
  }
  if (is.existy(req.followRedirect)) {
    options.followRedirects = req.followRedirect; // [sic] node-request compatibility
  }
  if (is.existy(req.maxRedirects)) {
    options.maxRedirects = req.maxRedirects;
  } else {
    options.maxRedirects = 10;
  }
  var result = internal.download(path, body, options);

  return new Response(result, req.encoding, req.json);
}

exports = request;
exports.request = request;
exports.Response = Response;

['delete', 'get', 'head', 'patch', 'post', 'put']
.forEach(function (method) {
  exports[method.toLowerCase()] = function (url, options) {
    if (typeof url === 'object') {
      options = url;
      url = undefined;
    } else if (typeof url === 'string') {
      options = extend({}, options, {url: url});
    }
    return request(extend({method: method.toUpperCase()}, options));
  };
});

module.exports = exports;
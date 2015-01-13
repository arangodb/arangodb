/*jshint globalstrict: true, sub: true */
/*global module, require */
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
var Buffer = require('buffer');
var extend = require('extend');
var httpErrors = require('http-errors');
var mediaTyper = require('media-typer');
var querystring = require('querystring');
var qs = require('qs');

function Response(res) {
  this.status = this.statusCode = res.code;
  this.message = res.message;
  this.body = this.rawBody = res.body;
  this.headers = res.headers;
  if (res.headers['content-type']) {
    var contentType = mediaTyper.parse(res.headers['content-type']);
    var types = [contentType.subtype, contentType.suffix];
    var isJson = types.indexOf('json') !== -1;
    var isXml = !isJson && types.indexOf('xml') !== -1;
    if (isJson || isXml || contentType.type === 'text') {
      this.body = this.rawBody.toString(contentType.parameters.charset || 'utf-8');
      if (isJson) {
        this.body = JSON.stringify(this.body);
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
  return (useQuerystring ? querystring : qs).stringify(query);
}

function request(req) {
  if (typeof req === 'string') {
    req = {url: req, method: 'GET'};
  }

  var url = req.url || req.uri;
  if (!url) {
    throw new Error('Request URL must not be empty.');
  }
  var query = querystringify(req.qs, req.useQuerystring);
  if (query) {
    url += '?' + query;
  }

  var headers = extend({}, headers);

  var contentType;
  var body = req.body;
  if (typeof body === 'string') {
    contentType = 'text/plain; charset=utf-8';
  } else if (body instanceof Buffer) {
    contentType = 'application/octet-stream';
  } else if (body && typeof body === 'object') {
    body = JSON.stringify(body);
    contentType = 'application/json';
  } else if (!body) {
    if (req.form) {
      body = querystringify(req.form);
      contentType = 'application/x-www-form-urlencoded';
    } else if (req.formData) {
      // body = formData(req.formData);
      // contentType = 'multipart/form-data';
      throw new Error('multipart encoding is currently not supported');
    } else if (req.multipart) {
      // body = multipart(req.multipart);
      // contentType = 'multipart/related';
      throw new Error('multipart encoding is currently not supported');
    }
  }
  headers['content-type'] = headers['content-type'] || contentType;

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

  var result = internal.download(url, body, {
    headers: headers,
    method: req.method,
    timeout: req.timeout,
    followRedirects: req.followRedirect, // [sic] node-request compatibility
    maxRedirects: req.maxRedirects,
    raw: true
  });

  var res = new Response(result);
  if (res.statusCode >= 400) {
    var status = httpErrors[res.statusCode] ? res.statusCode : 500;
    var err = new httpErrors[status](res.message);
    err.request = extend({}, req, {headers: headers, body: body});
    err.response = res;
    throw err;
  }
  return res;
}

request.Response = Response;

request.get = function (req, qs) {
  if (typeof req === 'string') {
    req = {url: req};
  }
  return request(extend({method: 'GET', qs: qs}, req));
};

request.head = function (req, qs) {
  if (typeof req === 'string') {
    req = {url: req};
  }
  return request(extend({method: 'HEAD', qs: qs}, req));
};

request.delete = function (req, qs) {
  if (typeof req === 'string') {
    req = {url: req};
  }
  return request(extend({method: 'DELETE', qs: qs}, req));
};

request.post = function (req, body) {
  if (typeof req === 'string') {
    req = {url: req};
  }
  return request(extend({method: 'POST', body: body}, req));
};

request.put = function (req, body) {
  if (typeof req === 'string') {
    req = {url: req};
  }
  return request(extend({method: 'PUT', body: body}, req));
};

request.patch = function (req, body) {
  if (typeof req === 'string') {
    req = {url: req};
  }
  return request(extend({method: 'PATCH', body: body}, req));
};

module.exports = request;
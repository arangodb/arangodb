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
var contentDisposition = require('content-disposition');
var querystring = require('querystring');
var qs = require('qs');

function Response(res, encoding) {
  this.status = this.statusCode = res.code;
  this.message = res.message;
  this.headers = res.headers ? res.headers : {};
  this.body = res.body;
  if (this.body && encoding !== null) {
    this.body = this.body.toString(encoding || 'utf-8');
  }
}

function parseForm(body) {
  return qs.parse(body);
}

function parseFormData(body, headers) {
  var parts = internal.requestParts({requestBody: body, headers: headers});
  var formData = {};
  parts.forEach(function (part) {
    var headers = part.headers || {};
    if (!headers['content-disposition']) {
      return; // Not form data
    }
    var disposition = contentDisposition.parse(headers['content-disposition']);
    if (disposition.name !== 'form-data' || !disposition.parameters.name) {
      return; // Not form data
    }
    var body = part.body;
    var contentType = mediaTyper.parse(headers['content-type'] || 'text/plain');
    if (contentType.type === 'text') {
      body = part.body.toString(contentType.parameters.charset || 'utf-8');
    } else if (contentType.type === 'multipart' && contentType.subtype === 'mixed') {
      body = parseMultipart(part.body, part.headers);
    } else {
      body = extend({}, part.headers, {body: part.body});
    }
    var name = disposition.parameters.name;
    if (formData.hasOwnProperty(name)) {
      if (!Array.isArray(formData[name])) {
        formData[name] = [formData[name]];
      }
      formData[name].push(body);
    } else {
      formData[name] = body;
    }
  });
  return formData;
}

function parseMultipart(body, headers) {
  var parts = internal.requestParts({requestBody: body, headers: headers});
  return parts.map(function (part) {
    return extend({}, part.headers, {body: part.body});
  });
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

  var url = req.url || req.uri;
  if (!url) {
    throw new Error('Request URL must not be empty.');
  }
  var query = querystringify(req.qs, req.useQuerystring);
  if (query) {
    url += '?' + query;
  }


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
      contentType = 'application/x-www-form-urlencoded';
      body = querystringify(req.form, req.useQuerystring);
    } else if (req.formData) {
      // contentType = 'multipart/form-data';
      // body = formData(req.formData);
      throw new Error('multipart encoding is currently not supported');
    } else if (req.multipart) {
      // contentType = 'multipart/related';
      // body = multipart(req.multipart);
      throw new Error('multipart encoding is currently not supported');
    }
  }

  var headers = extend({'content-type': contentType}, headers);

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
    returnBodyAsBuffer: true
  });

  var res = new Response(result, req.encoding);
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
request.parseForm = parseForm;
request.parseFormData = parseFormData;
request.parseMultipart = parseMultipart;

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
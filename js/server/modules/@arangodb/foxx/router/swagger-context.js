'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Router Swagger Context
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2015-2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const parseMediaType = require('media-typer').parse;
const tokenize = require('@arangodb/foxx/router/tokenize');

module.exports = class SwaggerContext {
  constructor(path) {
    if (!path) {
      path = '';
    }
    {
      let n = path.length - 1;
      if (path.charAt(n) === '/') {
        path = path.slice(0, n);
      }
      if (path.charAt(0) !== '/') {
        path = `/${path}`;
      }
    }
    this._headers = new Map();
    this._queryParams = new Map();
    this._bodyParam = null;
    this._responses = new Map();
    this._summary = null;
    this._description = null;
    this._deprecated = false;
    this.path = path;
    this._pathParams = new Map();
    this._pathParamNames = [];
    this._pathTokens = tokenize(path, this);
  }
  header(name, type, description) {
    this._headers.set(name, {type: type, description: description});
    return this;
  }
  pathParam(name, type, description) {
    this._pathParams.set(name, {type: type, description: description});
    return this;
  }
  queryParam(name, type, description) {
    this._queryParams.set(name, {type: type, description: description});
    return this;
  }
  body(type, description) {
    if (type === 'json') {
      type = 'application/json';
    } else if (type === 'text') {
      type = 'text/plain';
    } else if (type === 'binary') {
      type = 'application/octet-stream';
    }
    this._bodyParam = {
      type: type,
      mime: parseMediaType(typeof type === 'string' ? type : 'application/json'),
      description: description
    };
    return this;
  }
  response(status, type, description) {
    let statusCode = Number(status || undefined);
    if (Number.isNaN(statusCode)) {
      description = type;
      type = status;
      statusCode = 200;
    }
    this._responses.set(statusCode, {type: type, description: description});
    return this;
  }
  error(status, description) {
    return this.response(status, undefined, description);
  }
  summary(text) {
    this._summary = text;
    return this;
  }
  description(text) {
    this._description = text;
    return this;
  }
  deprecated(flag) {
    this._deprecated = typeof flag === 'boolean' ? flag : true;
    return this;
  }
  _merge(swaggerObj, pathOnly) {
    if (!pathOnly) {
      for (let header of swaggerObj._headers.entries()) {
        this._headers.set(header[0], header[1]);
      }
      for (let queryParam of swaggerObj._queryParams.entries()) {
        this._queryParams.set(queryParam[0], queryParam[1]);
      }
      for (let response of swaggerObj._responses.entries()) {
        this._responses.set(response[0], response[1]);
      }
      this._bodyParam = swaggerObj._bodyParam || this._bodyParam;
      this._deprecated = swaggerObj._deprecated || this._deprecated;
      this._description = swaggerObj._description || this._description;
      this._summary = swaggerObj._summary || this._summary;
    }
    if (this.path.charAt(this.path.length - 1) === '*') {
      this.path = this.path.slice(0, this.path.length - 1);
    }
    if (this.path.charAt(this.path.length - 1) === '/') {
      this.path = this.path.slice(0, this.path.length - 1);
    }
    this._pathTokens.pop();
    this._pathTokens = this._pathTokens.concat(swaggerObj._pathTokens);
    for (let pathParam of swaggerObj._pathParams.entries()) {
      let name = pathParam[0];
      let def = pathParam[1];
      if (this._pathParams.has(name)) {
        let baseName = name;
        let i = 2;
        let match = name.match(/(^.+)([0-9]+)$/i);
        if (match) {
          baseName = match[1];
          i = Number(match[2]);
        }
        while (this._pathParams.has(baseName + i)) {
          i++;
        }
        name = baseName + i;
      }
      this._pathParams.set(name, def);
      this._pathParamNames.push(name);
    }
    this.path = tokenize.reverse(this._pathTokens, this._pathParamNames);
  }
  _buildOperation() {
    const operation = {};
    if (this._deprecated) {
      operation.deprecated = this._deprecated;
    }
    if (this._description) {
      operation.description = this._description;
    }
    if (this._summary) {
      operation.summary = this._summary;
    }
    return operation;
  }
};

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
    this._response = null;
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
  response(type, description) {
    this._response = {type: type, description: description};
    return this;
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
};

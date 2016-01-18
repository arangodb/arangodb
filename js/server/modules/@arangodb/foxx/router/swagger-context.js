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

const joi = require('joi');
const mimeTypes = require('mime-types');
const mediaTyper = require('media-typer');
const joi2schema = require('joi-to-json-schema');
const tokenize = require('@arangodb/foxx/router/tokenize');

const DEFAULT_BODY_SCHEMA = joi.object().optional().meta({allowInvalid: true});
const DEFAULT_ERROR_SCHEMA = joi.object().keys({
  error: joi.allow(true).required(),
  errorNum: joi.number().integer().optional(),
  errorMessage: joi.string().optional(),
  code: joi.number().integer().optional()
});


module.exports = exports = class SwaggerContext {
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
    if (type === null) {
      this._bodyParam = {};
    } else {
      let contentType = 'application/json; charset=utf-8';
      if (type === 'binary') {
        contentType = 'application/octet-stream';
      } else if (typeof type !== 'object') {
        contentType = mimeTypes.contentType(type) || type;
      }
      const parsed = mediaTyper.parse(contentType);
      this._bodyParam = {
        type: type,
        description: description,
        contentType: {
          header: contentType,
          parsed: parsed,
          mime: mediaTyper.format({
            type: parsed.type,
            subtype: parsed.subtype,
            suffix: parsed.suffix
          })
        }
      };
    }
    return this;
  }

  response(status, type, description) {
    let statusCode = Number(status);
    if (!statusCode || Number.isNaN(statusCode)) {
      description = type;
      type = status;
      statusCode = 200;
    }
    if (type === null) {
      if (statusCode === 200) {
        this._responses.remove(200);
        statusCode = 204;
      }
      this._responses.set(statusCode, {});
    } else {
      let contentType = 'application/json; charset=utf-8';
      if (type === 'binary') {
        contentType = 'application/octet-stream';
      } else if (typeof type !== 'object') {
        contentType = mimeTypes.contentType(type) || type;
      }
      const parsed = mediaTyper.parse(contentType);
      this._responses.set(statusCode, {
        type: type,
        description: description,
        contentType: {
          header: contentType,
          parsed: parsed,
          mime: mediaTyper.format({
            type: parsed.type,
            subtype: parsed.subtype,
            suffix: parsed.suffix
          })
        }
      });
    }
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
      if (swaggerObj._bodyParam) {
        if (!this._bodyParam || swaggerObj._bodyParam.type !== DEFAULT_BODY_SCHEMA) {
          this._bodyParam = swaggerObj._bodyParam;
        }
      }
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
    const operation = {
      produces: [],
      parameters: []
    };
    if (this._deprecated) {
      operation.deprecated = this._deprecated;
    }
    if (this._description) {
      operation.description = this._description;
    }
    if (this._summary) {
      operation.summary = this._summary;
    }
    if (this._bodyParam) {
      operation.consumes = (
        this._bodyParam.contentType
        ? [this._bodyParam.contentType.mime]
        : []
      );
    }
    for (const response of this._responses.values()) {
      if (operation.produces.indexOf(response.contentType.mime) === -1) {
        operation.produces.push(response.contentType.mime);
      }
    }
    if (operation.produces.indexOf('application/json') === -1) {
      // ArangoDB errors use 'application/json'
      operation.produces.push('application/json');
    }

    for (const param of this._pathParams.entries()) {
      const name = param[0];
      const def = param[1];
      const parameter = (
        def.type && def.type.isJoi
        ? swaggerifyParam(def.type)
        : {type: 'string'}
      );
      parameter.name = name;
      parameter.in = 'path';
      parameter.required = true;
      if (def.description) {
        parameter.description = def.description;
      }
      operation.parameters.push(parameter);
    }

    for (const param of this._queryParams.entries()) {
      const name = param[0];
      const def = param[1];
      const parameter = (
        def.type && def.type.isJoi
        ? swaggerifyParam(def.type)
        : {type: 'string'}
      );
      parameter.name = name;
      parameter.in = 'query';
      if (def.description) {
        parameter.description = def.description;
      }
      operation.parameters.push(parameter);
    }

    for (const param of this._headers.entries()) {
      const name = param[0];
      const def = param[1];
      const parameter = (
        def.type && def.type.isJoi
        ? swaggerifyParam(def.type)
        : {type: 'string'}
      );
      parameter.name = name;
      parameter.in = 'header';
      if (def.description) {
        parameter.description = def.description;
      }
      operation.parameters.push(parameter);
    }

    if (this._bodyParam && this._bodyParam.contentType) {
      // TODO handle multipart and form-urlencoded
      const def = this._bodyParam;
      const schema = (
        def.type
        ? (def.type.schema || def.type)
        : null
      );
      const parameter = (
        schema && schema.isJoi
        ? swaggerifyBody(schema)
        : {schema: {type: 'string'}}
      );
      parameter.name = 'body';
      parameter.in = 'body';
      if (def.description) {
        parameter.description = def.description;
      }
      operation.parameters.push(parameter);
    }

    operation.responses = {
      default: {
        description: 'Unexpected error',
        schema: joi2schema(DEFAULT_ERROR_SCHEMA)
      }
    };

    for (const entry of this._responses.entries()) {
      const code = entry[0];
      const def = entry[1];
      const schema = (
        def.type
        ? (def.type.schema || def.type)
        : null
      );
      const response = {
        schema: (
          schema && schema.isJoi
          ? joi2schema(schema)
          : {type: 'string'}
        )
      };
      if (schema && schema.isJoi && schema._description) {
        response.description = schema._description;
      }
      if (def.description) {
        response.description = def.description;
      }
      operation.responses[code] = response;
    }

    return operation;
  }
};

exports.DEFAULT_BODY_SCHEMA = DEFAULT_BODY_SCHEMA;


function swaggerifyType(joi) {
  switch (joi._type) {
    default:
      return ['string'];
    case 'binary':
      return ['string', 'binary'];
    case 'boolean':
      return ['boolean'];
    case 'date':
      return ['string', 'date-time'];
    case 'func':
      return ['string'];
    case 'number':
      if (joi._tests.some(function (test) {
        return test.name === 'integer';
      })) {
        return ['integer'];
      }
      return ['number'];
    case 'array':
      return ['array'];
    case 'object':
      return ['object'];
    case 'string':
      if (joi._meta.some(function (meta) {
        return meta.secret;
      })) {
        return ['string', 'password'];
      }
      return ['string'];
  }
}

function swaggerifyParam(joi) {
  const param = {
    required: joi._presence === 'required',
    description: joi._description
  };
  let item = param;
  if (joi._meta.some(function (meta) {
    return meta.allowMultiple;
  })) {
    param.type = 'array';
    param.collectionFormat = 'multi';
    param.items = {};
    item = param.items;
  }
  const type = swaggerifyType(joi);
  item.type = type[0];
  if (type.length > 1) {
    item.format = type[1];
  }
  if (joi._valids._set) {
    item.enum = joi._valids._set;
  }
  if (joi._flags.hasOwnProperty('default')) {
    item.default = joi._flags.default;
  }
  return param;
}

function swaggerifyBody(joi) {
  return {
    required: joi._presence === 'required',
    description: joi._description,
    schema: joi2schema(joi)
  };
}

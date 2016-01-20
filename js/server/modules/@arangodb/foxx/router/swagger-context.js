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

const _ = require('lodash');
const joi = require('joi');
const assert = require('assert');
const mimeTypes = require('mime-types');
const mediaTyper = require('media-typer');
const joiToJsonSchema = require('joi-to-json-schema');
const tokenize = require('@arangodb/foxx/router/tokenize');

const MIME_JSON = 'application/json; charset=utf-8';
const MIME_BINARY = 'application/octet-stream';
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

  header(name, schema, description) {
    this._headers.set(name, {schema: schema, description: description});
    return this;
  }

  pathParam(name, schema, description) {
    this._pathParams.set(name, {schema: schema, description: description});
    return this;
  }

  queryParam(name, schema, description) {
    this._queryParams.set(name, {schema: schema, description: description});
    return this;
  }

  body(model, mimes, description) {
    if (
      typeof model === 'string'
      || (Array.isArray(model) && typeof model[0] === 'string')
    ) {
      description = mimes;
      mimes = model;
      model = undefined;
    }

    if (typeof mimes === 'string') {
      description = mimes;
      mimes = undefined;
    }

    let multiple = false;
    if (Array.isArray(model)) {
      if (model.length !== 1) {
        throw new Error(
          'Model must be a model or schema'
          + ' or an array containing exactly one model or schema.'
          + ' If you are trying to use multiple schemas'
          + ' try using joi.alternatives.'
        );
      }
      model = model[0];
      multiple = true;
    }

    if (!mimes) {
      mimes = [];
    }

    if (!mimes.length && model) {
      mimes.push(MIME_JSON);
    }

    const contentTypes = mimes.map(function (mime) {
      if (mime === 'binary') {
        mime = MIME_BINARY;
      }
      const contentType = mimeTypes.contentType(mime) || mime;
      const parsed = mediaTyper.parse(contentType);
      return mediaTyper.format(_.pick(parsed, [
        'type',
        'subtype',
        'suffix'
      ]));
    });

    if (model) {
      if (model.isJoi) {
        model = {schema: model};
      } else {
        model = _.clone(model);
      }
      if (model.schema && !model.schema.isJoi) {
        model.schema = joi.object(model.schema).required();
      }
      assert(
        !model.forClient || typeof model.forClient === 'function',
        `Request body model forClient handler must be a function, not ${typeof model.forClient}`
      );
      assert(
        !model.fromClient || typeof model.fromClient === 'function',
        `Request body model fromClient handler must be a function, not ${typeof model.fromClient}`
      );
      if (!model.forClient && typeof model.toClient === 'function') {
        console.log(
          `Found unexpected "toClient" method on request body model.`
          + ' Did you mean "forClient"?'
        );
      }
    }

    this._bodyParam = {
      model: model || null,
      multiple: multiple,
      contentTypes: contentTypes,
      description: description
    };
    return this;
  }

  response(status, model, mimes, description) {
    let statusCode = Number(status);
    if (!status || Number.isNaN(statusCode)) {
      description = mimes;
      mimes = model;
      model = status;
      statusCode = 200;
    }

    if (model === null) {
      if (statusCode === 200) {
        this._responses.delete(200);
        statusCode = 204;
      }
    }

    if (
      typeof model === 'string'
      || (Array.isArray(model) && typeof model[0] === 'string')
    ) {
      description = mimes;
      mimes = model;
      model = undefined;
    }

    if (typeof mimes === 'string') {
      description = mimes;
      mimes = undefined;
    }

    let multiple = false;
    if (Array.isArray(model)) {
      if (model.length !== 1) {
        throw new Error(
          'Model must be a model or schema'
          + ' or an array containing exactly one model or schema.'
          + ' If you are trying to use multiple schemas'
          + ' try using joi.alternatives.'
        );
      }
      model = model[0];
      multiple = true;
    }

    if (!mimes) {
      mimes = [];
    }

    if (!mimes.length && model) {
      mimes.push(MIME_JSON);
    }

    const contentTypes = mimes.map(function (mime) {
      if (mime === 'binary') {
        mime = MIME_BINARY;
      }
      const contentType = mimeTypes.contentType(mime) || mime;
      const parsed = mediaTyper.parse(contentType);
      return mediaTyper.format(_.pick(parsed, [
        'type',
        'subtype',
        'suffix'
      ]));
    });

    if (model) {
      if (model.isJoi) {
        model = {schema: model};
      } else {
        model = _.clone(model);
      }
      if (model.schema && !model.schema.isJoi) {
        model.schema = joi.object(model.schema).required();
      }
      assert(
        !model.forClient || typeof model.forClient === 'function',
        `Response body model forClient handler at ${statusCode} must be a function, not ${typeof model.forClient}`
      );
      assert(
        !model.fromClient || typeof model.fromClient === 'function',
        `Response body model fromClient handler at ${statusCode} must be a function, not ${typeof model.fromClient}`
      );
      if (!model.forClient && typeof model.toClient === 'function') {
        console.log(
          `Found unexpected "toClient" method on response body model at ${statusCode}.`
          + ' Did you mean "forClient"?'
        );
      }
    }

    this._responses.set(statusCode, {
      model: model || null,
      multiple: multiple,
      contentTypes: contentTypes,
      description: description
    });
    return this;
  }

  error(status, description) {
    return this.response(
      status,
      DEFAULT_ERROR_SCHEMA,
      null,
      description
    );
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
        this._bodyParam.contentTypes
        ? this._bodyParam.contentTypes.slice()
        : []
      );
    }
    for (const response of this._responses.values()) {
      if (!response.contentTypes) {
        continue;
      }
      for (const contentType of response.contentTypes) {
        if (operation.produces.indexOf(contentType) === -1) {
          operation.produces.push(contentType);
        }
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
        def.schema
        ? swaggerifyParam(def.schema.isJoi ? def.schema : joi.object(def.schema))
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
        def.schema
        ? swaggerifyParam(def.schema.isJoi ? def.schema : joi.object(def.schema))
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
        def.schema
        ? swaggerifyParam(def.schema.isJoi ? def.schema : joi.object(def.schema))
        : {type: 'string'}
      );
      parameter.name = name;
      parameter.in = 'header';
      if (def.description) {
        parameter.description = def.description;
      }
      operation.parameters.push(parameter);
    }

    if (this._bodyParam && this._bodyParam.contentTypes) {
      const def = this._bodyParam;
      const schema = (
        def.model
        ? (def.model.isJoi ? def.model : def.model.schema)
        : null
      );
      const parameter = (
        schema
        ? swaggerifyBody(schema, def.multiple)
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
        def.model
        ? (def.model.isJoi ? def.model : def.model.schema)
        : null
      );
      const response = {};
      if (def.contentTypes) {
        response.schema = (
          schema
          ? joi2schema(schema.isJoi ? schema : joi.object(schema), def.multiple)
          : {type: 'string'}
        );
      }
      if (schema && schema._description) {
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
  if (joi._valids._set && joi._valids._set.length) {
    item.enum = joi._valids._set;
  }
  if (joi._flags.hasOwnProperty('default')) {
    item.default = joi._flags.default;
  }
  return param;
}


function swaggerifyBody(joi, multiple) {
  return {
    required: joi._presence === 'required',
    description: joi._description,
    schema: joi2schema(joi, multiple)
  };
}


function joi2schema(schema, multiple) {
  if (multiple) {
    schema = joi.array().items(schema);
  }
  return joiToJsonSchema(schema);
}

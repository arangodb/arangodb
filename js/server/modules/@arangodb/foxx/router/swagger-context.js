'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2015-2016 ArangoDB GmbH, Cologne, Germany
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
const joi = require('joi');
const statuses = require('statuses');
const mimeTypes = require('mime-types');
const mediaTyper = require('media-typer');
const check = require('@arangodb/foxx/check-args');
const joiToJsonSchema = require('joi-to-json-schema');
const tokenize = require('@arangodb/foxx/router/tokenize');
const { uuidv4 } = require('@arangodb/crypto');

const MIME_JSON = 'application/json; charset=utf-8';
const DEFAULT_ERROR_SCHEMA = joi.object().keys({
  error: joi.allow(true).required(),
  errorNum: joi.number().integer().optional(),
  errorMessage: joi.string().optional(),
  code: joi.number().integer().optional()
});

const PARSED_JSON_MIME = (function (mime) {
  const contentType = mimeTypes.contentType(mime) || mime;
  const parsed = mediaTyper.parse(contentType);
  return mediaTyper.format(_.pick(parsed, [
    'type',
    'subtype',
    'suffix'
  ]));
}(MIME_JSON));

const repeat = (times, value) => {
  const arr = Array(times);
  for (let i = 0; i < times; i++) {
    arr[i] = value;
  }
  return arr;
};

module.exports = exports =
  class SwaggerContext {
    constructor (path) {
      if (!path) {
        path = '';
      }
      {
        const n = path.length - 1;
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
      this._tags = new Set();
      this._securitySchemes = new Map();
      this._security = new Map();
    }

    header (...args) {
      const argv = check(
        'endpoint.header',
        args,
        [['name', 'string'], ['schema', check.validateSchema], ['description', 'string']],
        [['name', 'string'], ['description', 'string']],
        [['name', 'string'], ['schema', check.validateSchema]],
        [['name', 'string']]
      );
      let schema = argv.schema;
      let description = argv.description;
      this._headers.set(argv.name.toLowerCase(), {schema, description});
      return this;
    }

    pathParam (...args) {
      const argv = check(
        'endpoint.pathParam',
        args,
        [['name', 'string'], ['schema', check.validateSchema], ['description', 'string']],
        [['name', 'string'], ['description', 'string']],
        [['name', 'string'], ['schema', check.validateSchema]],
        [['name', 'string']]
      );
      let schema = argv.schema;
      let description = argv.description;
      this._pathParams.set(argv.name, {schema, description});
      return this;
    }

    queryParam  (...args) {
      const argv = check(
        'endpoint.queryParam',
        args,
        [['name', 'string'], ['schema', check.validateSchema], ['description', 'string']],
        [['name', 'string'], ['description', 'string']],
        [['name', 'string'], ['schema', check.validateSchema]],
        [['name', 'string']]
      );
      let schema = argv.schema;
      let description = argv.description;
      this._queryParams.set(argv.name, {schema, description});
      return this;
    }

    body  (...args) {
      const argv = check(
        'endpoint.body',
        args,
        [['model', check.validateModel], ['mimes', check.validateMimes], ['description', 'string']],
        [['model', check.validateModel], ['description', 'string']],
        [['mimes', check.validateMimes], ['description', 'string']],
        [['model', check.validateModel], ['mimes', check.validateMimes]],
        [['model', check.validateModel]],
        [['mimes', check.validateMimes]],
        [['description', 'string']]
      );
      let model = argv.model;
      let mimes = argv.mimes;
      let description = argv.description;

      if (!model) {
        model = {multiple: false};
      }

      if (model.model === null) {
        this._bodyParam = {
          model: null,
          multiple: model.multiple,
          contentTypes: null,
        description};
        return this;
      }

      if (!mimes) {
        mimes = [];
      }

      if (!mimes.length && model.model) {
        mimes.push(PARSED_JSON_MIME);
      }

      this._bodyParam = {
        model: model.model,
        multiple: model.multiple,
        contentTypes: mimes,
      description};
      return this;
    }

    response (...args) {
      const argv = check(
        'endpoint.response',
        args,
        [['status', check.validateStatus], ['model', check.validateModel], ['mimes', check.validateMimes], ['description', 'string']],
        [['status', check.validateStatus], ['model', check.validateModel], ['description', 'string']],
        [['status', check.validateStatus], ['mimes', check.validateMimes], ['description', 'string']],
        [['status', check.validateStatus], ['model', check.validateModel], ['mimes', check.validateMimes]],
        [['model', check.validateModel], ['mimes', check.validateMimes], ['description', 'string']],
        [['status', check.validateStatus], ['model', check.validateModel]],
        [['status', check.validateStatus], ['mimes', check.validateMimes]],
        [['status', check.validateStatus], ['description', 'string']],
        [['model', check.validateModel], ['mimes', check.validateMimes]],
        [['model', check.validateModel], ['description', 'string']],
        [['mimes', check.validateMimes], ['description', 'string']],
        [['model', check.validateModel]],
        [['mimes', check.validateMimes]],
        [['description', 'string']]
      );
      let status = argv.status;
      let model = argv.model;
      let mimes = argv.mimes;
      let description = argv.description;

      if (!model) {
        model = {multiple: false};
      }

      if (!status) {
        status = model.model === null ? 204 : 200;
      }

      if (model.model === null) {
        this._responses.set(status, {
          model: null,
          multiple: model.multiple,
          contentTypes: null,
        description});
        return this;
      }

      if (!mimes) {
        mimes = [];
      }

      if (!mimes.length && model.model) {
        mimes.push(PARSED_JSON_MIME);
      }

      this._responses.set(status, {
        model: model.model,
        multiple: model.multiple,
        contentTypes: mimes,
      description});
      return this;
    }

    error (...args) {
      const argv = check(
        'endpoint.error',
        args,
        [['status', check.validateStatus], ['description', 'string']],
        [['status', check.validateStatus]]
      );
      let status = argv.status;
      let description = argv.description;
      this._responses.set(status, {
        model: DEFAULT_ERROR_SCHEMA,
        multiple: false,
        contentTypes: [PARSED_JSON_MIME],
        description
      });
      return this;
    }

    summary (text) {
      [text] = check(
        'endpoint.summary',
        [text],
        [['text', 'string']]
      );
      this._summary = text;
      return this;
    }

    description (text) {
      [text] = check(
        'endpoint.description',
        [text],
        [['text', 'string']]
      );
      this._description = text;
      return this;
    }

    tag (...tags) {
      tags = check(
        'endpoint.tag',
        tags,
        [...repeat(Math.max(1, tags.length), ['tag', 'string'])]
      );
      for (const tag of tags) {
        this._tags.add(tag);
      }
      return this;
    }

    securityScheme(...args) {
      const {type, options, description} = check(
        'endpoint.security',
        args,
        [
          ['type', 'string'],
          ['options', 'object'],
          ['description', 'string']
        ],
        [
          ['type', 'string'],
          ['options', 'object']
        ],
        [
          ['type', 'string'],
          ['description', 'string']
        ],
        [
          ['type', 'string']
        ]
      );
      const securityScheme = {...options, type};
      if (description) securityScheme.description = description;
      const id = securityScheme.id || uuidv4();
      delete securityScheme.id;
      this._securitySchemes.set(id, securityScheme);
      this._security.set(id, new Set());
      return this;
    }

    securityScope(...args) {
      const [id, ...scopes] = check(
        'endpoint.securityScope',
        args,
        [
          ['id', 'string'],
          ...repeat(Math.max(1, args.length - 1), ['scope', 'string'])
        ]
      );
      if (this._security.has(id) && this._security.get(id) !== false) {
        const security = this._security.get(id);
        for (const scope of scopes) {
          security.add(scope);
        }
      } else {
        this._security.set(id, new Set(scopes));
      }
      return this;
    }

    security(...args) {
      const [id, enabled = true] = check(
        'endpoint.security',
        args,
        [['id', 'string'], ['enabled', 'boolean']],
        [['id', 'string']]
      );
      if (enabled) {
        this._security.set(id, new Set());
      } else {
        this._security.set(id, false);
      }
      return this;
    }

    deprecated (...args) {
      const [flag] = check(
        'endpoint.summary',
        args,
        [['flag', 'boolean']],
        []
      );
      this._deprecated = typeof flag === 'boolean' ? flag : true;
      return this;
    }

    _merge (swaggerObj, pathOnly) {
      if (!pathOnly) {
        for (const header of swaggerObj._headers.entries()) {
          this._headers.set(header[0], header[1]);
        }
        for (const queryParam of swaggerObj._queryParams.entries()) {
          this._queryParams.set(queryParam[0], queryParam[1]);
        }
        for (const response of swaggerObj._responses.entries()) {
          this._responses.set(response[0], response[1]);
        }
        for (const tag of swaggerObj._tags) {
          this._tags.add(tag);
        }
        for (const [id, securityScheme] of swaggerObj._securitySchemes.entries()) {
          if (!this._securitySchemes.has(id)) {
            this._securitySchemes.set(id, securityScheme);
          }
        }
        for (const [id, scopes] of swaggerObj._security.entries()) {
          if (scopes === false) {
            this._security.set(id, false);
          } else if (this._security.has(id) && this._security.get(id) !== false) {
            const security = this._security.get(id);
            for (const scope of scopes) {
              security.add(scope);
            }
          } else {
            this._security.set(id, new Set(scopes));
          }
        }
        if (!this._bodyParam && swaggerObj._bodyParam) {
          this._bodyParam = swaggerObj._bodyParam;
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
      for (const pathParam of swaggerObj._pathParams.entries()) {
        let name = pathParam[0];
        const def = pathParam[1];
        if (this._pathParams.has(name)) {
          let baseName = name;
          let i = 2;
          const match = name.match(/(^.+)([0-9]+)$/i);
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

    _buildOperation () {
      const meta = {};
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
      if (this._tags) {
        operation.tags = Array.from(this._tags);
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
        500: {
          description: 'Default error response.',
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
        if (def.contentTypes && def.contentTypes.length) {
          response.schema = (
            schema
              ? joi2schema(schema, def.multiple)
              : {type: 'string'}
          );
        }
        if (schema && schema._description) {
          response.description = schema._description;
          delete response.schema.description;
        }
        if (def.description) {
          response.description = def.description;
        }
        if (!response.description) {
          const message = statuses[code];
          response.description = message ? `HTTP ${code} ${message}.` : (
            response.schema
              ? `Nondescript ${code} response.`
              : `Nondescript ${code} response without body.`
            );
        }
        operation.responses[code] = response;
      }

      if (this._securitySchemes.size) {
        meta.securitySchemes = this._securitySchemes;
      };
      operation.security = [];
      for (const [id, scopes] of this._security.entries()) {
        if (scopes !== false) {
          operation.security.push({[id]: [...scopes]});
        }
      }

      return {operation, meta};
    }
};

function swaggerifyType (joi) {
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
      if (joi._tests.some((test) => test.name === 'integer')) {
        return ['integer'];
      }
      return ['number'];
    case 'array':
      return ['array'];
    case 'object':
      return ['object'];
    case 'string':
      if (joi._meta.some((meta) => meta.secret)) {
        return ['string', 'password'];
      }
      return ['string'];
  }
}

function swaggerifyParam (joi) {
  const param = {
    required: joi._presence === 'required',
    description: joi._description || undefined
  };
  let item = param;
  if (joi._meta.some((meta) => meta.allowMultiple)) {
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

function swaggerifyBody (joi, multiple) {
  return {
    required: joi._presence === 'required',
    description: joi._description || undefined,
    schema: joi2schema(joi, multiple)
  };
}

function joi2schema (schema, multiple) {
  if (multiple) {
    schema = joi.array().items(schema);
  }
  return joiToJsonSchema(schema);
}

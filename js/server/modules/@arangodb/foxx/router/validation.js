'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
const assert = require('assert');
const typeIs = require('type-is');
const mediaTyper = require('media-typer');
const requestParts = require('internal').requestParts;

exports.validateParams = function validateParams (typeDefs, rawParams, type) {
  if (!type) {
    type = 'parameter';
  }
  const params = {};
  for (const entry of typeDefs) {
    const name = entry[0];
    const def = entry[1];
    if (def.schema && typeof def.schema.validate === 'function') {
      const result = def.schema.validate(rawParams[name]);
      if (result.error) {
        const e = result.error;
        e.message = e.message.replace(/^"value"/, `${type} "${name}"`);
        throw e;
      }
      params[name] = result.value;
    } else {
      params[name] = rawParams[name];
    }
  }
  return params;
};

exports.parseRequestBody = function parseRequestBody (def, req) {
  let body = req.body;

  if (!def.contentTypes) {
    assert(!body || !body.length, 'Unexpected request body');
    return null;
  }

  const indicatedType = req.get('content-type');
  let actualType;

  if (indicatedType) {
    for (const candidate of def.contentTypes) {
      if (typeIs.is(indicatedType, candidate)) {
        actualType = candidate;
        break;
      }
    }
  }

  if (!actualType) {
    actualType = def.contentTypes[0] || indicatedType;
  }

  const parsedType = mediaTyper.parse(actualType);
  actualType = mediaTyper.format(_.pick(parsedType, ['type', 'subtype', 'suffix']));

  if (parsedType.type === 'multipart') {
    body = requestParts(req._raw);
  }

  let handler;
  for (const entry of req.context.service.types.entries()) {
    const key = entry[0];
    const value = entry[1];
    let match;
    if (key instanceof RegExp) {
      match = actualType.test(key);
    } else if (typeof key === 'function') {
      match = key(actualType);
    } else {
      match = typeIs.is(key, actualType);
    }
    if (match && value.fromClient) {
      handler = value;
      break;
    }
  }

  if (handler) {
    body = handler.fromClient(body, req, parsedType);
  }

  return body;
};

exports.validateRequestBody = function validateRequestBody (def, req) {
  let body = req.body;

  let schema = def.model && (def.model.schema || def.model);
  if (!schema) {
    return body;
  }

  if (schema.isJoi) {
    if (def.multiple) {
      schema = joi.array().items(schema).required();
    }

    const result = schema.validate(body);

    if (result.error) {
      result.error.message = result.error.message.replace(/^"value"/, 'request body');
      throw result.error;
    }

    body = result.value;
  }

  if (def.model && def.model.fromClient) {
    if (def.multiple) {
      body = body.map((body) => def.model.fromClient(body));
    } else {
      body = def.model.fromClient(body);
    }
  }

  return body;
};

'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
const assert = require('assert');
const typeIs = require('type-is');
const ct = require('content-type');
const requestParts = require('internal').requestParts;

exports.validateParams = function validateParams (typeDefs, rawParams, type) {
  if (!type) {
    type = 'parameter';
  }
  const params = {};
  for (const [name, def] of typeDefs) {
    let value = rawParams[name];
    if (def.fromClient) {
      value = def.fromClient(value);
    }
    const result = def.validate(value);
    if (result.error) {
      const e = result.error;
      e.message = e.message.replace(/^"value"/, `${type} "${name}"`);
      throw e;
    }
    params[name] = result.value;
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

  actualType = ct.parse(actualType);

  if (actualType.type.startsWith('multipart/')) {
    body = requestParts(req._raw);
  }

  let handler;
  for (const [key, value] of req.context.service.types.entries()) {
    let match;
    if (key instanceof RegExp) {
      match = actualType.type.test(key);
    } else if (typeof key === 'function') {
      match = key(actualType.type, actualType.parameters);
    } else {
      match = typeIs.is(key, actualType.type);
    }
    if (match && value.fromClient) {
      handler = value;
      break;
    }
  }

  if (handler) {
    body = handler.fromClient(body, req, actualType.parameters);
  }

  return body;
};

exports.validateRequestBody = function validateRequestBody (def, req) {
  let body = req.body;

  const result = def.model.validate(body);
  if (result.error) {
    result.error.message = result.error.message.replace(/^"value"/, 'request body');
    throw result.error;
  }
  body = result.value;

  if (def.model.fromClient) {
    body = def.model.fromClient(body);
  }

  return body;
};

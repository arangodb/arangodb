'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Router Validation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2016 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

exports.validateParams = function validateParams(typeDefs, rawParams) {
  const params = {};
  for (let entry of typeDefs) {
    const name = entry[0];
    const def = entry[1];
    if (def.type.isJoi) {
      const result = def.type.validate(rawParams[name]);
      if (result.error) {
        throw result.error;
      }
      params[name] = result.value;
    }
  }
  return params;
};

exports.validateRequestBody = function validateRequestBody(def, rawBody) {
  if (def.type === null) {
    if (rawBody && rawBody.length) {
      throw new Error('Unexpected request body');
    }
    return null;
  }

  const isJson = def.mime.subtype === 'json' || def.mime.suffix === 'json';
  const charset = def.mime.parameters.charset;

  if (
    (def.mime.type !== 'text' && !isJson) ||
    (charset && charset.toLowerCase() !== 'utf-8')
  ) {
    return rawBody;
  }

  const textBody = rawBody.toString('utf-8');

  if (!isJson) {
    return textBody;
  }

  const jsonBody = JSON.parse(textBody);
  const schema = def.type.schema || def.type;

  if (!schema.isJoi) {
    return jsonBody;
  }

  const result = schema.validate(jsonBody);

  if (result.error) {
    result.error.message = result.error.message.replace(/^"value"/, '"request body"');
    throw result.error;
  }

  if (schema === def.type || typeof def.type.fromClient !== 'function') {
    return result.value;
  }

  return def.type.fromClient(result.value);
};

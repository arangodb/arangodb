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
const Ajv = require('ajv');
const dd = require('dedent');
const il = require('@arangodb/util').inline;
const joi = require('joi');
const util = require('util');
const statuses = require('statuses');
const mimeTypes = require('mime-types');
const ct = require('content-type');
const ArangoError = require('@arangodb').ArangoError;
const ERROR_BAD_PARAMETER = require('@arangodb').errors.ERROR_BAD_PARAMETER;
const ajv = new Ajv({coerceTypes: true, useDefaults: true, strictSchema: false});

function normalizeMimeType (mime) {
  if (mime === 'binary') {
    mime = 'application/octet-stream';
  }
  const contentType = mimeTypes.contentType(mime) || mime;
  const parsed = ct.parse(contentType);
  return parsed.type;
}

function isJoiSchema (value, expectOpaque) {
  if (!value) return false;
  if (value.isJoi) return true;
  if (expectOpaque) return false;
  if (typeof value !== "object") return false;
  if (Array.isArray(value)) return false;
  const values = Object.values(value);
  if (!values.length) return false;
  return values.every(value => isJoiSchema(value, true));
}

function createSchemaValidator (schema, optional = false) {
  if (schema.isJoi) {
    return (value) => schema.validate(value);
  }
  if (schema === false) {
    return (value) => {
      if (value === null || value === undefined || value === "") {
        return {value: undefined, error: null};
      }
      return {value, error: new Error('"value" is forbidden')};
    };
  }
  const validator = ajv.compile(schema);
  return (value) => {
    if (optional && value === undefined) {
      return {value: undefined, error: null};
    }
    const valid = validator(value);
    if (valid) return {value, error: null};
    return {value, error: new Error(
      ajv.errorsText(validator.errors, {dataVar: `"value"`})
    )};
  };
}

function runValidation (methodName, paramName, type, value) {
  const warnings = [];
  if (typeof type === 'string') {
    let isa = typeof value;
    if (Array.isArray(value) && type !== 'object') {
      isa = 'array';
    }
    if (isa !== type) {
      return {
        value,
        error: new Error(`${paramName} must be a ${type}, not ${isa}`),
        warnings
      };
    }
    return {value, error: null, warnings};
  } else if (typeof type === 'function') {
    const result = type(value);
    if (result.warnings) {
      for (const warning of result.warnings) {
        warnings.push(warning.replace(/^"value"/, paramName));
      }
    }
    if (result.error) {
      result.error.message = result.error.message
      .replace(/^"value"/, paramName);
      return {value: result.value, error: result.error, warnings};
    }
    return {value: result.value, error: null, warnings};
  } else {
    throw new Error(il`
      Unknown type for parameter ${paramName} in ${methodName}:
      ${util.inspect(type)}
    `);
  }
}

module.exports = exports = function (methodName, values, ...variations) {
  for (let i = values.length - 1; i >= 0; i--) {
    if (values[i] !== undefined) {
      break;
    }
    values.pop();
  }
  const warnings = [];
  const validations = new Map();
  let output;
  let error = null;
  for (const variation of variations) {
    if (variation.length > values.length) {
      if (!error) {
        const [name] = variation[values.length];
        error = new Error(`${name} is required`);
        error.default = true;
        error.variation = variation;
      }
      continue;
    }
    output = [];
    let err = null;
    const multis = new Set();
    for (let i = 0; i < variation.length; i++) {
      const [name, type] = variation[i];
      const value = values[i];
      if (!validations.has(value)) {
        validations.set(value, new Map());
      }
      const results = validations.get(value);
      if (!results.has(type)) {
        results.set(type, runValidation(
          methodName,
          name,
          type,
          value
        ));
      }
      const result = results.get(type);
      for (const warning of result.warnings) {
        warning.variation = variation;
        warnings.push(warning);
      }
      if (result.error) {
        err = result.error;
        break;
      }
      output[i] = result.value;
      if (multis.has(name)) {
        output[name].push(result.value);
      } else if (hasOwnProperty.call(output, name)) {
        multis.add(name);
        output[name] = [output[name], result.value];
      } else {
        output[name] = result.value;
      }
    }
    if (err) {
      if (error && !error.default) {
        continue;
      }
      error = err;
      error.variation = variation;
    } else {
      error = null;
      break;
    }
  }
  if (warnings.length || error) {
    for (const warning of warnings) {
      const signature = warning.variation.map(([name]) => name).join(', ');
      console.warnLines(`${methodName}(${signature}): ${warning}`);
    }
    if (error) {
      const signature = error.variation.map(([name]) => name).join(', ');
      throw Object.assign(
        new ArangoError({
          errorNum: ERROR_BAD_PARAMETER.code,
          errorMessage: dd`
            ${ERROR_BAD_PARAMETER.message}
            ${error.message}
            Method: ${methodName}(${signature})
            Arguments: ${util.inspect(values, {simpleJoi: true, depth: 3})}
          `
        }), {cause: error}
      );
    }
  }
  return output;
};

exports.validateMiddleware = function (value) {
  if (value && (typeof value === 'function' || typeof value.register === 'function')) {
    return {value, error: null};
  }
  return {value, error: new Error('"value" is not a valid middleware')};
};

exports.validateMountable = function (value) {
  if (value && value.isFoxxRouter) {
    return {value, error: null};
  }
  return exports.validateMiddleware(value);
};

exports.validateStatus = function (value) {
  let status = value;
  if (typeof status === 'string') {
    try {
      status = statuses(status);
    } catch (e) {
      return {value, error: Object.assign(
          new Error('"value" must be a valid status name.'),
          {cause: e}
      )};
    }
  }
  status = Number(status);
  if (Number.isNaN(status)) {
    return {value, error: new Error('"value" must be a number')};
  }
  return {value: status, error: null};
};

exports.validateSchema = function (schema = true, optional = false) {
  if (schema === null) {
    return {
      value: {schema: false, validate: createSchemaValidator(false, optional)},
      error: null
    };
  }
  if (isJoiSchema(schema)) {
    if (schema.isJoi) {
      return {
        value: {schema, validate: createSchemaValidator(schema, optional)},
        error: null
      };
    }
    try {
      schema = joi.object(schema).required();
    } catch (e) {
      return {value: schema, error: Object.assign(
        new Error('"value" must be a schema'),
        {cause: e}
      )};
    }
    return {
      value: {schema, validate: createSchemaValidator(schema, optional)},
      error: null
    };
  }
  const valid = ajv.validateSchema(schema);
  if (!valid) {
    return {value: schema, error: new Error(
      ajv.errorsText(ajv.errors, {dataVar: `"value"`})
    )};
  }
  return {
    value: {schema, validate: createSchemaValidator(schema, optional)},
    error: null
  };
};

exports.validateModel = function (value) {
  let model = value;
  const warnings = [];

  let multiple = false;
  if (Array.isArray(model)) {
    if (model.length !== 1) {
      return {value, error: new Error(il`
        "value" must be a model or schema
        or an array containing exactly one model or schema.
      `), warnings};
    }
    model = model[0];
    multiple = true;
  }

  let index = multiple ? '[0]' : '';
  if (model && typeof model !== "object" && typeof model !== "boolean") {
    return {value, error: new Error(
      `"value"${index} must be an object or boolean`
    ), warnings};
  }

  let schemaIndex = index;
  if (model && !model.isJoi && Object.keys(model).length && (
    Object.values(model).some((prop) => typeof prop === "function")
    || model.schema !== undefined
  )) {
    model = {...model};
    schemaIndex = `${index}.schema`;
  } else {
    model = {schema: model};
  }

  const result = exports.validateSchema(model.schema, model.optional);
  if (result.error) {
    result.error.message = result.error.message
      .replace(/^"value"/, `"value"${schemaIndex}`);
    return {value, error: result.error, warnings};
  }
  model.schema = result.value.schema;
  if (model.schema !== true || !model.validate) {
    model.validate = result.value.validate;
  }

  if (!model.forClient && typeof model.toClient === 'function') {
    warnings.push(il`
      "value"${index} has unexpected "toClient" method.
      Did you mean "forClient"?
    `);
  }
  if (model.forClient && typeof model.forClient !== 'function') {
    return {value, error: new Error(il`
      "value"${index}.forClient must be a function,
      not ${typeof model.forClient}.
    `), warnings};
  }
  if (model.fromClient && typeof model.fromClient !== 'function') {
    return {value, error: new Error(il`
      "value"${index}.fromClient must be a function,
      not ${typeof model.fromClient}.
    `), warnings};
  }

  if (multiple) {
    let baseModel = model;
    model = {schema: baseModel.schema};
    if (baseModel.fromClient) {
      model.fromClient = (value) => Array.isArray(value)
        ? value.map(item => baseModel.fromClient(item))
        : value;
    }
    if (baseModel.forClient) {
      model.forClient = (value) => Array.isArray(value)
        ? value.map(item => baseModel.forClient(item))
        : value;
    }
    if (baseModel.schema.isJoi) {
      model.schema = joi.array().items(baseModel.schema).required();
    } else {
      model.schema = {type: "array", items: baseModel.schema};
    }
    if (baseModel.schema === true && baseModel.validate) {
      const validateItem = baseModel.validate;
      model.validate = (value) => {
        if (!Array.isArray(value)) {
          return {value, error: new Error(`"value" must be an array.`)};
        }
        const arr = Array(value.length);
        for (const item of value) {
          const result = validateItem(item);
          if (result.error) {
            return result;
          }
          arr.push(result.value);
        }
        return {value: arr, error: null};
      };
    } else {
      model.validate = createSchemaValidator(model.schema, model.optional);
    }
  }

  return {value: model, error: null};
};

exports.validateMimes = function (value) {
  if (!Array.isArray(value)) {
    return {value, error: new Error(`"value" must be an array.`)};
  }
  const mimes = [];
  for (let i = 0; i < value.length; i++) {
    try {
      mimes.push(normalizeMimeType(value[i]));
    } catch (e) {
      return {value, error: Object.assign(
          new Error(`"value"[${i}] must be a valid MIME type.`),
          {cause: e}
      )};
    }
  }
  return {value: mimes, error: null};
};

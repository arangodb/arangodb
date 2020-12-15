'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2013-2014 triAGENS GmbH, Cologne, Germany
// / Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
// / @author Lucas Dohmen
// / @author Michael Hackstein
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const SwaggerDocs = require('@arangodb/foxx/legacy/swaggerDocs').Docs;
const joi = require('joi');
const _ = require('lodash');
const internal = require('@arangodb/foxx/legacy/internals');
const toJSONSchema = require('@arangodb/foxx/legacy/schema').toJSONSchema;
const is = require('@arangodb/is');
const UnprocessableEntity = require('http-errors').UnprocessableEntity;
const UnauthorizedError = require('@arangodb/foxx/legacy/authentication').UnauthorizedError;

function createBodyParamExtractor (rootElement, paramName, allowInvalid) {
  var extractElement;

  if (rootElement) {
    extractElement = function (req) {
      return req.body()[paramName];
    };
  } else {
    extractElement = function (req) {
      return req.body();
    };
  }

  if (!allowInvalid) {
    return extractElement;
  }

  return function (req) {
    try {
      return extractElement(req);
    } catch (e) {
      return {};
    }
  };
}

function createModelInstantiator (Model, allowInvalid) {
  var multiple = is.array(Model);
  Model = multiple ? Model[0] : Model;
  var instantiate = function (raw) {
    if (!allowInvalid) {
      raw = validateOrThrow(raw, Model.prototype.schema, allowInvalid);
    }
    return new Model(raw);
  };
  if (!multiple) {
    return instantiate;
  }
  return function (raw) {
    return _.map(raw, instantiate);
  };
}

function isJoi (schema) {
  if (!schema || typeof schema !== 'object' || is.array(schema)) {
    return false;
  }

  if (schema.isJoi) { // shortcut for pure joi schemas
    return true;
  }

  return Object.keys(schema).some(function (key) {
    return schema[key].isJoi;
  });
}

function validateOrThrow (raw, schema, allowInvalid, validateOptions) {
  if (!isJoi(schema)) {
    return raw;
  }
  var result = joi.validate(raw, schema, validateOptions);
  if (result.error && !allowInvalid) {
    throw new UnprocessableEntity(result.error.message.replace(/^"value"/, 'Request body'));
  }
  return result.value;
}

class RequestContext {

  // //////////////////////////////////////////////////////////////////////////////
  // / JSF_foxx_RequestContext_initializer
  // / @brief Context of a Request Definition
  // /
  // / Used for documenting and constraining the routes.
  // //////////////////////////////////////////////////////////////////////////////

  constructor (executionBuffer, models, route, path, rootElement, constraints, extensions) {
    this.path = path;
    this.route = route;
    this.typeToRegex = {
      int: '/[0-9]+/',
      integer: '/[0-9]+/',
      string: '/[^/]+/'
    };
    this.rootElement = rootElement;
    this.constraints = constraints;

    this.docs = new SwaggerDocs(this.route.docs, models);

    var attr;
    var extensionWrapper = function (scope, func) {
      return function () {
        func.apply(this, arguments);
        return this;
      }.bind(scope);
    };
    for (attr in extensions) {
      if (extensions.hasOwnProperty(attr)) {
        this[attr] = extensionWrapper(this, extensions[attr]);
      }
    }
    executionBuffer.applyEachFunction(this);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContext_pathParam
  // //////////////////////////////////////////////////////////////////////////////

  pathParam (paramName, attributes) {
    var url = this.route.url,
      urlConstraint = url.constraint || {},
      type = attributes.type,
      required = attributes.required,
      description = attributes.description,
      constraint, regexType, cfg;

    if (attributes.isJoi) {
      type = attributes;
      required = undefined;
      description = undefined;
    }

    constraint = type;
    regexType = type;

    if (type) {
      if (typeof required === 'boolean') {
        constraint = required ? constraint.required() : constraint.optional();
      }
      if (typeof description === 'string') {
        constraint = constraint.description(description);
      }
      this.constraints.urlParams[paramName] = constraint;
      cfg = constraint.describe();
      if (is.array(cfg)) {
        cfg = cfg[0];
        type = 'string';
      } else {
        type = cfg.type;
      }
      required = Boolean(cfg.flags && cfg.flags.presence === 'required');
      description = cfg.description;
      if (
        type === 'number' &&
        _.isArray(cfg.rules) &&
        _.some(cfg.rules, function (rule) {
          return rule.name === 'integer';
        })
      ) {
        type = 'integer';
      }
      if (_.has(this.typeToRegex, type)) {
        regexType = type;
      } else {
        regexType = 'string';
      }
    }

    urlConstraint[paramName] = this.typeToRegex[regexType];
    if (!urlConstraint[paramName]) {
      throw new Error('Illegal attribute type: ' + regexType);
    }
    this.route.url = internal.constructUrlObject(url.match, urlConstraint, url.methods[0]);
    this.docs.addPathParam(paramName, description, type, required);
    return this;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContext_queryParam
  // //////////////////////////////////////////////////////////////////////////////

  queryParam (paramName, attributes) {
    var type = attributes.type,
      required = attributes.required,
      description = attributes.description,
      allowMultiple = attributes.allowMultiple,
      constraint, cfg;

    if (attributes.isJoi) {
      type = attributes;
      required = undefined;
      description = undefined;
      allowMultiple = undefined;
    }

    constraint = type;

    if (type) {
      if (typeof required === 'boolean') {
        constraint = required ? constraint.required() : constraint.optional();
      }
      if (typeof description === 'string') {
        constraint = constraint.description(description);
      }
      if (typeof allowMultiple === 'boolean') {
        constraint = constraint.meta({allowMultiple: allowMultiple});
      }
      this.constraints.queryParams[paramName] = constraint;
      cfg = constraint.describe();
      if (is.array(cfg)) {
        cfg = cfg[0];
        type = 'string';
      } else {
        type = cfg.type;
      }
      required = Boolean(cfg.flags && cfg.flags.presence === 'required');
      description = cfg.description;
      if (cfg.meta) {
        if (!is.array(cfg.meta)) {
          cfg.meta = [cfg.meta];
        }
        _.each(cfg.meta, function (meta) {
          if (meta && typeof meta.allowMultiple === 'boolean') {
            allowMultiple = meta.allowMultiple;
          }
        });
      }
      if (
        type === 'number' &&
        _.isArray(cfg.rules) &&
        _.some(cfg.rules, function (rule) {
          return rule.name === 'integer';
        })
      ) {
        type = 'integer';
      }
    }

    this.docs.addQueryParam(
      paramName,
      description,
      type,
      required,
      Boolean(allowMultiple)
    );
    return this;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContext_bodyParam
  // //////////////////////////////////////////////////////////////////////////////

  bodyParam (paramName, attributes) {
    var type = attributes.type,
      description = attributes.description,
      allowInvalid = attributes.allowInvalid,
      validateOptions = {},
      cfg, construct;

    if (attributes.isJoi) {
      type = attributes;
      description = undefined;
      allowInvalid = undefined;
    }

    if (!type) {
      construct = function (raw) {
        return raw;
      };
    } else if (typeof type === 'function' || is.array(type)) {
      // assume ModelOrSchema is a Foxx Model
      construct = createModelInstantiator(type, allowInvalid);
    } else {
      if (!type.isJoi) {
        type = joi.object().keys(type).required();
      }
      if (typeof allowInvalid === 'boolean') {
        type = type.meta({allowInvalid: allowInvalid});
      }
      if (typeof description === 'string') {
        type = type.description(description);
      }
      cfg = type.describe();
      description = cfg.description;
      if (cfg.meta) {
        if (!is.array(cfg.meta)) {
          cfg.meta = [cfg.meta];
        }
        _.each(cfg.meta, function (meta) {
          if (meta && typeof meta.allowInvalid === 'boolean') {
            allowInvalid = meta.allowInvalid;
          }
        });
      }
      if (cfg.options) {
        if (!is.array(cfg.options)) {
          cfg.options = [cfg.options];
        }
        _.each(cfg.options, function (options) {
          Object.assign(validateOptions, options);
        });
      }
      construct = function (raw) {
        return validateOrThrow(raw, type, allowInvalid, validateOptions);
      };
    }

    this.docs.addBodyParam(
      paramName,
      description,
      toJSONSchema(paramName, is.array(type) ? type[0] : type)
    );
    this.route.action.bodyParams.push({
      extract: createBodyParamExtractor(this.rootElement, paramName, allowInvalid),
      paramName: paramName,
      construct: construct
    });
    return this;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContext_summary
  // //////////////////////////////////////////////////////////////////////////////

  summary (summary) {
    if (summary.length > 8192) {
      throw new Error("Summary can't be longer than 8192 characters");
    }
    this.docs.addSummary(summary);
    return this;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContext_notes
  // //////////////////////////////////////////////////////////////////////////////

  notes () {
    var notes = Array.prototype.join.call(arguments, '\n');
    this.docs.addNotes(notes);
    return this;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContext_errorResponse
  // //////////////////////////////////////////////////////////////////////////////
  errorResponse (errorClass, code, reason, errorHandler) {
    this.route.action.errorResponses.push({
      errorClass: errorClass,
      code: code,
      reason: reason,
      errorHandler: errorHandler
    });
    this.route.docs.errorResponses.push({code: code, reason: reason});
    return this;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContext_onlyIf
  // //////////////////////////////////////////////////////////////////////////////
  onlyIf (check) {
    this.route.action.checks.push({
      check: check
    });
    return this;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContext_onlyIfAuthenticated
  // //////////////////////////////////////////////////////////////////////////////
  onlyIfAuthenticated (code, reason) {
    var check;

    check = function (req) {
      if (
        !(req.session && req.session.get('uid')) // new and shiny
        && !(req.user && req.currentSession) // old and busted
      ) {
        throw new UnauthorizedError();
      }
    };

    if (is.notExisty(code)) {
      code = 401;
    }
    if (is.notExisty(reason)) {
      reason = 'Not authorized';
    }

    this.onlyIf(check);
    this.errorResponse(UnauthorizedError, code, reason);

    return this;
  }
}

class RequestContextBuffer {
  constructor () {
    this.applyChain = [];
  }
}

Object.assign(RequestContextBuffer.prototype, {
  applyEachFunction(target) {
    _.each(this.applyChain, function (x) {
      target[x.functionName].apply(target, x.argumentList);
    });
  }
});

_.each([

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContextBuffer_pathParam
  // //////////////////////////////////////////////////////////////////////////////
  'pathParam',
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContextBuffer_queryParam
  // //////////////////////////////////////////////////////////////////////////////
  'queryParam',

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContextBuffer_errorResponse
  // //////////////////////////////////////////////////////////////////////////////
  'errorResponse',

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContextBuffer_onlyIf
  // //////////////////////////////////////////////////////////////////////////////
  'onlyIf',

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_RequestContextBuffer_onlyIfAuthenticated
  // //////////////////////////////////////////////////////////////////////////////
  'onlyIfAuthenticated'
], function (functionName) {
  Object.assign(RequestContextBuffer.prototype[functionName] = function () {
    this.applyChain.push({
      functionName: functionName,
      argumentList: arguments
    });
    return this;
  });
});

RequestContext.Buffer = RequestContextBuffer;

module.exports = exports = RequestContext;
exports.RequestContext = RequestContext;
exports.RequestContextBuffer = RequestContextBuffer;

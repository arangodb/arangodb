/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx internals
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Lucas Dohmen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var _ = require("underscore"),
  is = require("org/arangodb/is"),
  actions = require("org/arangodb/actions"),
  constructUrlObject,
  constructNickname,
  constructRoute,
  constructPathParamDoc,
  constructQueryParamDoc,
  constructErrorResponseDoc;

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_internals_constructUrlObject
/// @brief create a new url object
///
/// This creates a new `UrlObject`.
///
/// ArangoDB uses a certain structure we refer to as `UrlObject`.  With the
/// following function (which is only internal, and not exported) you can create
/// an UrlObject with a given URL, a constraint and a method. For example:
///
/// *Examples*
///
/// @code
///     internal.constructUrlObject('/lecker/gans', null, 'get');
/// @endcode
////////////////////////////////////////////////////////////////////////////////

constructUrlObject = function (url, constraint, method) {
  'use strict';
  var urlObject = {};

  if (is.noString(url)) {
    throw new Error("URL has to be a String");
  }

  urlObject.match = url;
  urlObject.methods = [method];

  if (is.truthy(constraint)) {
    urlObject.constraint = constraint;
  }

  return urlObject;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_internals_constructNickname
/// @brief Construct a swagger-compatible nickname from a httpMethod and URL
////////////////////////////////////////////////////////////////////////////////

constructNickname = function (httpMethod, url) {
  'use strict';
  return (httpMethod + "_" + url)
    .replace(/\W/g, '_')
    .replace(/((_){2,})/g, '_')
    .toLowerCase();
};

constructRoute = function (method, route, callback, controller, constraints) {
  'use strict';
  return {
    url: constructUrlObject(route, undefined, method),
    action: {
      callback: function (req, res) {
        if (constraints) {
          try {
            _.each({
              urlParameters: constraints.urlParams,
              parameters: constraints.queryParams
            }, function (paramConstraints, paramsPropertyName) {
              var params = req[paramsPropertyName];
              _.each(paramConstraints, function (constraint, paramName) {
                var result = constraint.validate(params[paramName]);
                params[paramName] = result.value;
                if (result.error) {
                  result.error.message = 'Invalid value for "' + paramName + '": ' + result.error.message;
                  throw result.error;
                }
              });
            });
          } catch (err) {
            actions.resultBad(req, res, actions.HTTP_BAD, err.message);
            return;
          }
        }
        _.each(controller.injectors, function (injector, key) {
          if (_.has(controller.injected, key)) {
            return;
          }
          if (typeof injector === 'function') {
            controller.injected[key] = injector();
          } else {
            controller.injected[key] = injector;
          }
        });
        callback(req, res, controller.injected);
      }
    },
    docs: {
      parameters: [],
      errorResponses: [],
      httpMethod: method.toUpperCase()
    }
  };
};

constructPathParamDoc = function (paramName, description, dataType) {
  'use strict';
  return {
    paramType: "path",
    name: paramName,
    description: description,
    dataType: dataType
  };
};

constructQueryParamDoc = function (paramName, description, dataType, required, allowMultiple) {
  'use strict';
  return {
    paramType: "query",
    name: paramName,
    description: description,
    dataType: dataType,
    required: required,
    allowMultiple: allowMultiple
  };
};

constructErrorResponseDoc = function (code, reason) {
  'use strict';
  return {
    code: code,
    reason: reason
  };
};

exports.constructUrlObject = constructUrlObject;
exports.constructNickname = constructNickname;
exports.constructRoute = constructRoute;
exports.constructPathParamDoc = constructPathParamDoc;
exports.constructQueryParamDoc = constructQueryParamDoc;
exports.constructErrorResponseDoc = constructErrorResponseDoc;

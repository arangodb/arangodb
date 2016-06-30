'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Foxx internals
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Lucas Dohmen
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var _ = require('lodash'),
  is = require('@arangodb/is');

// //////////////////////////////////////////////////////////////////////////////
  // / JSF_foxx_internals_constructUrlObject
  // / @brief create a new url object
  // /
  // / This creates a new `UrlObject`.
  // /
  // / ArangoDB uses a certain structure we refer to as `UrlObject`.  With the
  // / following function (which is only internal, and not exported) you can create
  // / an UrlObject with a given URL, a constraint and a method. For example:
  // /
  // / *Examples*
  // /
  // / @code
  // /     internal.constructUrlObject('/lecker/gans', null, 'get')
  // / @endcode
  // //////////////////////////////////////////////////////////////////////////////

function constructUrlObject (url, constraint, method) {
  var urlObject = {};

  if (is.noString(url)) {
    throw new Error('URL has to be a String');
  }

  urlObject.match = url;
  urlObject.methods = [method];

  if (is.truthy(constraint)) {
    urlObject.constraint = constraint;
  }

  return urlObject;
}

function constructRoute (method, route, callback, controller, constraints) {
  var res = {};
  res.url = constructUrlObject(route, undefined, method);
  res.docs = {
    parameters: [],
    errorResponses: [],
    httpMethod: method.toUpperCase()
  };
  res.action = {};
  res.action.callback = function (req, res) {
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
  };
  if (constraints) {
    res.action.constraints = constraints;
  }
  res.action.errorResponses = [];
  res.action.bodyParams = [];
  res.action.checks = [];
  return res;
}

exports.constructUrlObject = constructUrlObject;
exports.constructRoute = constructRoute;

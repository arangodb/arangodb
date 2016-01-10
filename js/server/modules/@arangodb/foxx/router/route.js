'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Router Route
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
const SwaggerContext = require('@arangodb/foxx/router/swagger-context');

const DEFAULT_BODY_SCHEMA = joi.object().optional().meta({allowInvalid: true});


module.exports = class Route extends SwaggerContext {
  constructor(methods, path, handler, name) {
    if (typeof path !== 'string') {
      name = handler;
      handler = path;
      path = '/';
    }
    super(path);
    this._methods = methods;
    this._handler = handler;
    this.name = name;
    if (['POST', 'PUT', 'PATCH'].some(function (method) {
      return methods.indexOf(method) !== -1;
    })) {
      this._bodyParam = {type: DEFAULT_BODY_SCHEMA};
    }
  }
};

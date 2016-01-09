'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Router Middleware
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

const SwaggerContext = require('@arangodb/foxx/router/swagger-context');

module.exports = class Middleware extends SwaggerContext {
  constructor(path, fn) {
    if (typeof path !== 'string') {
      fn = path;
      path = '/';
    }
    if (path.charAt(path.length - 1) !== '*') {
      if (path.charAt(path.length - 1) !== '/') {
        path += '/';
      }
      path += '*';
    }
    super(path);
    this._middleware = fn;
    this._handler = fn;
    if (typeof fn.register === 'function') {
      this._handler = fn.register(this) || fn;
    }
  }
};

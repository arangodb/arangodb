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

const BODYLESS_METHODS = ['DELETE', 'GET', 'HEAD', 'OPTIONS'];
const SwaggerContext = require('@arangodb/foxx/router/swagger-context');

module.exports =
  class Route extends SwaggerContext {
    constructor (methods, path, handler, name) {
      if (!path) {
        path = '/';
      }
      super(path);
      this._methods = methods;
      if (Array.isArray(handler)) {
        this._handler = (req, res) => {
          let i = 0;
          let result, hasResult;
          const next = (err) => {
            if (err) {
              res.throw(err);
            }
            const fn = handler[i];
            i += 1;
            if (i < handler.length) {
              const temp = fn(req, res, next);
              if (!hasResult) {
                hasResult = true;
                result = temp;
              }
            } else {
              result = fn(req, res);
              hasResult = true;
            }
          };
          next();
          return result;
        };
      } else {
        this._handler = handler;
      }
      this.name = name;
      if (!methods.some(
          (method) => BODYLESS_METHODS.indexOf(method) === -1
        )) {
        this.body(null);
      }
    }

    _PRINT (ctx) {
      ctx.output += `[FoxxRoute: "${this.path}"]`;
    }
};

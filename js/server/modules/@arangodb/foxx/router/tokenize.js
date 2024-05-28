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

const check = require('@arangodb/foxx/check-args');

const MATCH_STRINGS_MODEL = check.validateSchema({type: 'string'}).value;
const $_WILDCARD = Symbol.for('@@wildcard'); // catch-all suffix
const $_TERMINAL = Symbol.for('@@terminal'); // terminal -- routes be here
const $_PARAM = Symbol.for('@@parameter'); // named parameter (no routes here, like static part)

function reverse (pathTokens, pathParamNames) {
  const path = [];
  let i = 0;
  for (const token of pathTokens) {
    if (token === $_PARAM) {
      path.push(':' + pathParamNames[i]);
      i++;
    } else if (token === $_WILDCARD) {
      path.push('*');
    } else if (token !== $_TERMINAL) {
      path.push(token);
    }
  }
  return '/' + path.join('/');
}

module.exports = Object.assign(
  function tokenize (path, ctx) {
    if (path === '/') {
      return [$_TERMINAL];
    }
    const tokens = path.slice(1).split('/').map((name) => {
      if (name === '*') {
        return $_WILDCARD;
      }
      if (name.charAt(0) !== ':') {
        return name;
      }
      name = name.slice(1);
      ctx._pathParamNames.push(name);
      ctx._pathParams.set(name, MATCH_STRINGS_MODEL);
      return $_PARAM;
    });
    if (tokens[tokens.length - 1] !== $_WILDCARD) {
      tokens.push($_TERMINAL);
    }
    return tokens;
  }, {
    WILDCARD: $_WILDCARD,
    TERMINAL: $_TERMINAL,
    PARAM: $_PARAM,
    reverse: reverse
  }
);

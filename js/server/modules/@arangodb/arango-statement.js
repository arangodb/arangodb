/* jshint strict: false */
/* global AQL_PARSE, AQL_EXPLAIN, AQL_EXECUTE */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoStatement
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

module.isSystem = true;

var ArangoStatement = require('@arangodb/arango-statement-common').ArangoStatement;
var GeneralArrayCursor = require('@arangodb/arango-cursor').GeneralArrayCursor;
const ArangoQueryStreamCursor = require('internal').ArangoQueryStreamCursor;

// //////////////////////////////////////////////////////////////////////////////
// / @brief parse a query and return the results
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.parse = function () {
  var result = AQL_PARSE(this._query);

  return {
    bindVars: result.parameters,
    collections: result.collections,
    ast: result.ast
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief explain a query and return the results
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.explain = function (options) {
  let opts = this._options || { };
  if (typeof opts === 'object' && typeof options === 'object') {
    Object.keys(options).forEach(function (o) {
      // copy options
      opts[o] = options[o];
    });
  }
  return AQL_EXPLAIN(this._query, this._bindVars, opts);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief execute the query
// /
// / This will return a cursor with the query results in case of success.
// //////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.execute = function () {
  var opts = this._options || { };
  if (typeof opts === 'object') {
    opts.count = this._doCount;
    if (this._cache !== undefined) {
      opts.cache = this._cache;
    }
    if (opts.stream) {
      return new ArangoQueryStreamCursor(this._query, this._bindVars, opts);
    }
  }
  // {json:[docs], stats:{}, profile:{}, warnings:{}, cached:true}
  var result = AQL_EXECUTE(this._query, this._bindVars, opts);
  return new GeneralArrayCursor(result.json, 0, null, result);
};

exports.ArangoStatement = ArangoStatement;

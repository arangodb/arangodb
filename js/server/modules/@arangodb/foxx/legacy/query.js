'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Foxx queries
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 triAGENS GmbH, Cologne, Germany
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
// / @author Alan Plum
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var _ = require('lodash'),
  db = require('@arangodb').db;

exports.createQuery = function createQuery (cfg) {
  if (!cfg) {
    cfg = {};
  } else if (typeof cfg === 'string' || typeof cfg.toAQL === 'function') {
    cfg = {query: cfg};
  }

  var query = cfg.query,
    params = cfg.params,
    context = cfg.context,
    Model = cfg.model,
    defaults = cfg.defaults,
    transform = cfg.transform;

  if (params === false) {
    params = [];
  } else if (params && !Array.isArray(params)) {
    params = [params];
  }

  if (params && !params.every(function (v) {return typeof v === 'string';})) {
    throw new Error('Argument names must be a string, an array of strings or false.');
  }

  if (!query || (typeof query !== 'string' && typeof query.toAQL !== 'function')) {
    throw new Error('Expected query to be a string or a QueryBuilder instance.');
  }

  if (context && typeof context.collectionName !== 'function') {
    throw new Error('Expected context to have a method "collectionName".');
  }

  if (Model && typeof Model !== 'function') {
    throw new Error('Expected model to be a constructor function.');
  }

  if (transform && typeof transform !== 'function') {
    throw new Error('Expected transform to be a function.');
  }

  return function query () {
    var args = Array.prototype.slice.call(arguments);
    var vars;
    if (params) {
      vars = {};
      params.forEach(function (name) {
        vars[name] = args.shift();
      });
    } else {
      vars = args.shift();
    }
    vars = Object.assign({}, defaults, vars);
    if (context) {
      _.each(vars, function (value, key) {
        if (key.charAt(0) === '@') {
          vars[key] = context.collectionName(value);
        }
      });
    }
    var result = db._query(cfg.query, vars).toArray();
    if (Model) {
      result = _.map(result, function (data) {
        return new Model(data);
      });
    }
    args.unshift(result);
    return transform ? transform.apply(null, args) : result;
  };
};

/*global exports, require */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var _ = require('underscore'),
  db = require('org/arangodb').db;

exports.createQuery = function createQuery (cfg) {
  'use strict';

  if (!cfg) {
    cfg = {};
  } else if (typeof cfg === 'string' || typeof cfg.toAQL === 'function') {
    cfg = {query: cfg};
  }

  var query = cfg.query,
    context = cfg.context,
    Model = cfg.model,
    defaults = cfg.defaults,
    transform = cfg.transform;

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

  return function query(vars, trArgs) {
    vars = _.extend({}, defaults, vars);
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

    return transform ? transform(result, trArgs) : result;
  };
};

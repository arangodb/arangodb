/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Tasks
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2011-2013 triagens GmbH, Cologne, Germany
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
// / @author Wilfried Goesgens
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var arangosh = require('@arangodb/arangosh');
var arangodb = require('@arangodb');
var throwBadParameter = arangodb.throwBadParameter;
var db = internal.db;

var API = '/_api/tasks';

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a new task
// //////////////////////////////////////////////////////////////////////////////
var getTask = function (options) {
  var URL = API;

  if (typeof (options) === 'object') {
    if (!options.hasOwnProperty('id')) {
      throwBadParameter('options has to contain an id property.');
    }
    URL += '/' + encodeURIComponent(options.id);
  } else {
    if (typeof (options) !== 'undefined') {
      URL += '/' + encodeURIComponent(options);
    }
  }
  var requestResult = db._connection.GET(URL);

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

var registerTask = function (options) {
  var URL = API;
  var requestResult;

  if (typeof (options) !== 'object') {
    throwBadParameter('options has to be an object.');
  }

  if (typeof (options.command) === 'function') {
    options.command = '(' + String(options.command) + ')(params)';
  }
  if (options.hasOwnProperty('id')) {
    URL += '/' + options.id;
    requestResult = db._connection.PUT(URL, options);
  } else {
    requestResult = db._connection.POST(URL, options);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

var unregisterTask = function (options) {
  var deleteId;
  if (typeof (options) === 'object') {
    if (!options.hasOwnProperty('id')) {
      throwBadParameter('options has to contain an id property.');
    }
    deleteId = options.id;
  } else {
    if (typeof (options) === 'undefined') {
      throwBadParameter('options has to contain an id.');
    }
    deleteId = options;
  }
  var requestResult = db._connection.DELETE(API + '/' + deleteId);

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

exports.register = registerTask;
exports.unregister = unregisterTask;
exports.get = getTask;

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

var API = '/_api/control_pregel';

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a new pregel execution
// //////////////////////////////////////////////////////////////////////////////
var startExecution = function(options, algo) {
  /*var URL = API;

  if (typeof (options) === 'object') {
    if (!options.hasOwnProperty('id')) {
      throwBadParameter('options has to contain an id property.');
    }
    URL += '/' + encodeURIComponent(options.id);
  } else {
    if (typeof (options) !== 'undefined') {
      URL += '/' + encodeURIComponent(options);
    }
  }*/
  var first = options.vertexCollections && options.vertexCollections instanceof Array
  && options.edgeCollection && typeof options.edgeCollection === 'string';
  var second = typeof options.graphName === 'string' && typeof options.algorithm === 'string';
  var third = typeof options === 'string' && typeof algo === 'string';
  if (!first && !second && !third) {
    throw "Invalid parameters, either {vertexCollections:['',..], edgeCollection: ''} or {graphName:'<graph>'} or graph name";
  }
  if (third) {
    options = {graphName:options, algorithm:algo};
  }
  var requestResult = db._connection.POST(API, JSON.stringify(options));
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

var getExecutionStatus = function (executionID) {
  var URL = API + '/' + encodeURIComponent(executionID);
  var requestResult = db._connection.GET(URL);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

var cancelExecution = function (executionID) {
  var URL = API + '/' + encodeURIComponent(executionID);
  var requestResult = db._connection.DELETE(URL);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

exports.start = startExecution;
exports.status = getExecutionStatus;
exports.cancel = cancelExecution;

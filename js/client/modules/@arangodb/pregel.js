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

var API = '/_api/pregel';

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a new pregel execution
// //////////////////////////////////////////////////////////////////////////////
function startExecution(options) {
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
  var second = options.graphName &&  typeof options.graphName === 'string'
  if (!first && !second) {
    throw "Invalid parameters, either {vertexCollections:['',..], edgeCollection: ''} or {graphName:'<graph>'} or graph name";
  }
  var requestResult = db._connection.POST(API, JSON.stringify(options));
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

function getExecutionStatus(executionID) {
  var requestResult = db._connection.GET(API + '/' + executionID);
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

function cancelExecution(executionID) {
  
  var requestResult = db._connection.DELETE(API + '/' + executionID);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

exports.start = startExecution;
exports.status = getExecutionStatus;
exports.cancel = cancelExecution;

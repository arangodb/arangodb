/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
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
// / @author Simon Grätzer
// / @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arangosh = require('@arangodb/arangosh');
const arangodb = require('@arangodb');
const throwBadParameter = arangodb.throwBadParameter;
const db = internal.db;

const API = '/_api/control_pregel';
const HISTORY_API = '/_api/control_pregel/history';

let buildMessage = function (algo, second, params) {
  let valid = false;
  let message = {algorithm: algo};

  if (typeof algo === 'string' && second) {
    var first = second.vertexCollections && second.vertexCollections instanceof Array
      && second.edgeCollections && second.edgeCollections instanceof Array;
    if (first) {
      message.vertexCollections = second.vertexCollections;
      message.edgeCollections = second.edgeCollections;
      valid = true;
    } else if (typeof second === 'object' && typeof second.graphName === 'string') {
      message.graphName = second.graphName;
      valid = true;
    } else if (typeof second === 'string' && typeof algo === 'string') {
      message.graphName = second;
      valid = true;
    }
  }
  
  if (params && typeof params === 'object') {
    message.params = params;
  }

  if (!valid) {
    throw "Invalid parameters, either {vertexCollections:['',..], edgeCollections: ['',..]}" +
          " or {graphName:'<graph>'} or graph name";
  }

  return message;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a new pregel execution
// //////////////////////////////////////////////////////////////////////////////
var startExecution = function(algo, second, params) {
  let message = buildMessage(algo, second, params);
  let requestResult = db._connection.POST(API, message);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

var getExecutionStatus = function (executionID) {
  let URL = API;
  if (executionID !== undefined) {
    URL += '/' + encodeURIComponent(executionID);
  }
  let requestResult = db._connection.GET(URL);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

var cancelExecution = function (executionID) {
  var URL = API + '/' + encodeURIComponent(executionID);
  var requestResult = db._connection.DELETE(URL);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

// Test whether the pregel execution with executionID
// is still busy.
//
// With the current API this the only sensible test
// that can be offered: the executionID might have
// disappeared or never existed, but then the execution
// is not busy.
const isBusy = function (executionID) {
  let status = getExecutionStatus(executionID);
  return status === status.state === "loading" ||
    status.state === "running" ||
    status.state === "storing";
};

/*
 * Pregel History module section
 */
const getExecutionHistory = (executionID) => {
  let URL = HISTORY_API;
  if (executionID !== undefined) {
    URL += '/' + encodeURIComponent(executionID);
  }
  let requestResult = db._connection.GET(URL);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

const removeExecutionHistory = (executionID) => {
  let URL = HISTORY_API;
  if (executionID !== undefined) {
    URL += '/' + encodeURIComponent(executionID);
  }
  let requestResult = db._connection.DELETE(URL);
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

/*
 * Exports section
 */

exports.buildMessage = buildMessage;
exports.start = startExecution;
exports.status = getExecutionStatus;
exports.cancel = cancelExecution;
exports.isBusy = isBusy;

/*
 * Exports pregel history section
 */

exports.history = getExecutionHistory;
exports.removeHistory = removeExecutionHistory;

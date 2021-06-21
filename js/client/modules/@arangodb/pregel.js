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

var internal = require('internal');
var arangosh = require('@arangodb/arangosh');
var arangodb = require('@arangodb');
var throwBadParameter = arangodb.throwBadParameter;
var db = internal.db;

var API = '/_api/control_pregel';

let buildMessage = function(algo, second, params) {
  let valid = false;
  let message = { algorithm: algo };

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

exports.buildMessage = buildMessage;
exports.start = startExecution;
exports.status = getExecutionStatus;
exports.cancel = cancelExecution;
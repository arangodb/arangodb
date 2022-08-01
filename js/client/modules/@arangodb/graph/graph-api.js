'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph API wrapper
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// @author Michael Hackstein
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const GRAPH_PREFIX = '/_api/gharial/';
const _ = require('lodash');
const arangosh = require('@arangodb/arangosh');
const {prepareEdgeRequestBody, applyInsertParameterToUrl} = require("@arangodb/arango-collection-helper");
const {errors, db} = require('internal');
const ArangoError = require('@arangodb').ArangoError;

exports.edgeCollectionAPIWrapper = (graph, collection) => {
  const baseUrl = `${GRAPH_PREFIX}${graph.__name}/edge/${collection.name()}`;
  const wrapper = {};
  _.each(_.functionsIn(collection), function (functionName) {
    switch (functionName) {
      case "insert":
      case "save":
        // Overwrite Insert
        wrapper[functionName] = (from, to, data, options) => {
          require("internal").print(from, to, data, options);
          if (data === undefined) {
            data = from;
            options = to;
          } else {
            data = prepareEdgeRequestBody(from, to, data);
          }
          if (data === undefined || typeof data !== 'object') {
            throw new ArangoError({
              errorNum: errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
              errorMessage: errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.message
            });
          }
          if (options === undefined) {
            options = {};
          }
          const url = applyInsertParameterToUrl(baseUrl, options);
          let headers = {};
          if (options.transactionId) {
            headers['x-arango-trx-id'] = options.transactionId;
          }


          require("internal").print(data, url, headers);
          let requestResult = db._connection.POST(
            url, data, headers
          );

          arangosh.checkRequestResult(requestResult);

          return options.silent ? true : requestResult;
        };
        break;
      default:
        // Pass-through original implementation
        wrapper[functionName] = function () {
          return collection[functionName].apply(collection, arguments);
        };
        break;
    }

  });
  return wrapper;
};

exports.vertexCollectionAPIWrapper = (collection) => {
};

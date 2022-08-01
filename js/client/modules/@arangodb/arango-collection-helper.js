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

const {errors} = require('internal');
const ArangoError = require('@arangodb').ArangoError;

////////////////////////////////////////////////////////////////////////////////
/// @brief append some boolean parameter to a URL
////////////////////////////////////////////////////////////////////////////////

let appendBoolParameter = function (url, name, val) {
  if (url.indexOf('?') === -1) {
    url += '?';
  } else {
    url += '&';
  }
  url += name + (val ? '=true' : '=false');
  return url;
};

let appendOverwriteModeParameter = function (url, mode) {
  if (mode) {
    if (url.indexOf('?') === -1) {
      url += '?';
    } else {
      url += '&';
    }
    url += 'overwriteMode=' + mode;
  }
  return url;
};

exports.prepareEdgeRequestBody = (from, to, data) => {
  if (typeof data === 'object' && Array.isArray(data)) {
    throw new ArangoError({
      errorNum: errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
      errorMessage: errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.message
    });
  }
  if (data === undefined || data === null || typeof data !== 'object') {
    throw new ArangoError({
      errorNum: errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
      errorMessage: errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.message
    });
  }

  if (typeof from === 'object' && from.hasOwnProperty('_id')) {
    from = from._id;
  }

  if (typeof to === 'object' && to.hasOwnProperty('_id')) {
    to = to._id;
  }

  data._from = from;
  data._to = to;
  return data;
};

exports.applyInsertParameterToUrl = (url, options) => {
  // the following parameters are optional, so we only append them if necessary
  for (const key of ["mergeObjects", "keepNull", "waitForSync",
    "skipDocumentValidation", "returnNew", "returnOld",
    "silent", "overwrite", "isRestore"]) {
    if (options.hasOwnProperty(key)) {
      url = appendBoolParameter(url, key, options[key]);
    }
  }

  if (options.overwriteMode) {
    url = appendOverwriteModeParameter(url, options.overwriteMode);
  }
  return url;
}
;

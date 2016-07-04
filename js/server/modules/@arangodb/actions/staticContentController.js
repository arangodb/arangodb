/* jshint strict: false, unused: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief deliver static content from collection
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
// / @author Dr. Frank Celler
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');
var actions = require('@arangodb/actions');

// //////////////////////////////////////////////////////////////////////////////
// / @brief locate content from collection
// //////////////////////////////////////////////////////////////////////////////

function locateContentFromCollection (req, options) {
  var path;
  var collection;

  if (!options.hasOwnProperty('contentCollection')) {
    return null;
  }

  collection = arangodb.db._collection(options.contentCollection);

  if (collection === null) {
    return null;
  }

  path = '/' + req.suffix.join('/');

  return collection.firstExample({
    path: path,
    prefix: options.prefix,
  application: options.application });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief locate content from collection
// //////////////////////////////////////////////////////////////////////////////

exports.head = function (req, res, options, next) {
  var content;

  content = locateContentFromCollection(req, options);

  if (content === null) {
    res.responseCode = actions.HTTP_NOT_IMPLEMENTED;
    res.contentType = 'text/plain';
  } else {
    res.responseCode = actions.HTTP_OK;
    res.contentType = content.contentType || 'text/html';
  }

  res.body = '';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief get request
// //////////////////////////////////////////////////////////////////////////////

exports.get = function (req, res, options, next) {
  var content;

  content = locateContentFromCollection(req, options);

  if (content === null) {
    res.responseCode = actions.HTTP_NOT_IMPLEMENTED;
    res.contentType = 'text/plain';
    res.body = "path '" + req.suffix.join('/') + "' not implemented";
  } else {
    res.responseCode = actions.HTTP_OK;
    res.contentType = content.contentType || 'text/html';
    res.body = content.content || '';
  }
};

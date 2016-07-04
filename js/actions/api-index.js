/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief querying and managing indexes
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
// / @author Achim Brandt
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');
var actions = require('@arangodb/actions');

var API = '_api/index';

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_index
// //////////////////////////////////////////////////////////////////////////////

function get_api_indexes (req, res) {
  var name = req.parameters.collection;
  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var list = [], ids = {}, indexes = collection.getIndexes(), i;

  for (i = 0;  i < indexes.length;  ++i) {
    var index = indexes[i];

    list.push(index);
    ids[index.id] = index;
  }

  var result = { indexes: list, identifiers: ids };

  actions.resultOk(req, res, actions.HTTP_OK, result);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_reads_index
// //////////////////////////////////////////////////////////////////////////////

function get_api_index (req, res) {

  // .............................................................................
  // /_api/index?collection=<collection-name>
  // .............................................................................

  if (req.suffix.length === 0) {
    get_api_indexes(req, res);
  }

  // .............................................................................
  // /_api/index/<collection-name>/<index-identifier>
  // .............................................................................
  else if (req.suffix.length === 2) {
    var name = decodeURIComponent(req.suffix[0]);
    var collection = arangodb.db._collection(name);

    if (collection === null) {
      actions.collectionNotFound(req, res, name);
      return;
    }

    var iid = decodeURIComponent(req.suffix[1]);
    try {
      var index = collection.index(name + '/' + iid);
      if (index !== null) {
        actions.resultOk(req, res, actions.HTTP_OK, index);
        return;
      }
    } catch (err) {
      if (err.errorNum === arangodb.ERROR_ARANGO_INDEX_NOT_FOUND ||
        err.errorNum === arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        actions.indexNotFound(req, res, collection, iid);
        return;
      }
      throw err;
    }
  } else {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expect GET /' + API + '/<index-handle>');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_index_geo
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_index_hash
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_index_skiplist
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_index_fulltext
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_index
// //////////////////////////////////////////////////////////////////////////////

function post_api_index (req, res) {
  if (req.suffix.length !== 0) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expecting POST /' + API + '?collection=<collection-name>');
    return;
  }

  var name = req.parameters.collection;
  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }

  // inject collection name into body
  if (body.collection === undefined) {
    body.collection = name;
  }

  // create the index
  var index = collection.ensureIndex(body);
  actions.resultOk(req, res, index.isNewlyCreated ? actions.HTTP_CREATED : actions.HTTP_OK, index);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_index_delete
// //////////////////////////////////////////////////////////////////////////////

function delete_api_index (req, res) {
  if (req.suffix.length !== 2) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expect DELETE /' + API + '/<index-handle>');
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var iid = parseInt(decodeURIComponent(req.suffix[1]), 10);
  var dropped = collection.dropIndex(name + '/' + iid);

  if (dropped) {
    actions.resultOk(req, res, actions.HTTP_OK, { id: name + '/' + iid });
  } else {
    actions.indexNotFound(req, res, collection, name + '/' + iid);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief reads or creates an index
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API,

  callback: function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_index(req, res);
      } else if (req.requestType === actions.DELETE) {
        delete_api_index(req, res);
      } else if (req.requestType === actions.POST) {
        post_api_index(req, res);
      } else {
        actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

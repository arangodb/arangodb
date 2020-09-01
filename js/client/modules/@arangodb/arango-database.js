/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDatabase
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triagens GmbH, Cologne, Germany
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
// / @author Achim Brandt
// / @author Dr. Frank Celler
// / @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var arangosh = require('@arangodb/arangosh');

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

var ArangoCollection;
var ArangoView;

function ArangoDatabase (connection) {
  Object.defineProperty(this, "_dbProperties", {
    enumerable: false,
    writable: true
  });
  this._dbProperties = null;
  this._connection = connection;
}

exports.ArangoDatabase = ArangoDatabase;

// load after exporting ArangoDatabase
ArangoCollection = require('@arangodb/arango-collection').ArangoCollection;
ArangoView = require('@arangodb/arango-view').ArangoView;
var ArangoError = require('@arangodb').ArangoError;
var ArangoStatement = require('@arangodb/arango-statement').ArangoStatement;
let ArangoTransaction = require('@arangodb/arango-transaction').ArangoTransaction;

// //////////////////////////////////////////////////////////////////////////////
// / @brief index id regex
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.indexRegex = /^([a-zA-Z0-9\-_]+)\/([0-9]+)$/;

// //////////////////////////////////////////////////////////////////////////////
// / @brief key regex
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.keyRegex = /^([a-zA-Z0-9_:\-@\.\(\)\+,=;\$!\*'%])+$/;

// //////////////////////////////////////////////////////////////////////////////
// / @brief append the waitForSync parameter to a URL
// //////////////////////////////////////////////////////////////////////////////

let appendSyncParameter = function (url, waitForSync) {
  if (waitForSync) {
    if (url.indexOf('?') === -1) {
      url += '?';
    } else {
      url += '&';
    }
    url += 'waitForSync=true';
  }
  return url;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief append some boolean parameter to a URL
// //////////////////////////////////////////////////////////////////////////////

let appendBoolParameter = function (url, name, val) {
  if (url.indexOf('?') === -1) {
    url += '?';
  } else {
    url += '&';
  }
  url += name + (val ? '=true' : '=false');
  return url;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the base url for collection usage
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._collectionurl = function (id) {
  if (id === undefined) {
    return '/_api/collection';
  }

  return '/_api/collection/' + encodeURIComponent(id);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the base url for view usage
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._viewurl = function (id) {
  if (id === undefined) {
    return '/_api/view';
  }

  return '/_api/view/' + encodeURIComponent(id);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the base url for document usage
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._documenturl = function (id, expectedName) {
  var s = id.split('/');

  if (s.length !== 2) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  } else if (expectedName !== undefined && expectedName !== '' && s[0] !== expectedName) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code,
      errorMessage: internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.message
    });
  }

  if (ArangoDatabase.keyRegex.exec(s[1]) === null) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  return '/_api/document/' + encodeURIComponent(s[0]) + '/' + encodeURIComponent(s[1]);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the base url for index usage
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._indexurl = function (id, expectedName) {
  if (typeof id === 'string') {
    var pa = ArangoDatabase.indexRegex.exec(id);

    if (pa === null && expectedName !== undefined && !id.startsWith(expectedName + '/')) {
      id = expectedName + '/' + id;
    }
  } else if (typeof id === 'number' && expectedName !== undefined) {
    // stringify a numeric id
    id = expectedName + '/' + id;
  }

  var s = id.split('/');

  if (s.length !== 2) {
    // invalid index handle
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code,
      errorMessage: internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message
    });
  } else if (expectedName !== undefined && expectedName !== '' && s[0] !== expectedName) {
    // index handle does not match collection name
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code,
      errorMessage: internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.message
    });
  }

  return '/_api/index/' + encodeURIComponent(s[0]) + '/' + encodeURIComponent(s[1]);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints the help for ArangoDatabase
// //////////////////////////////////////////////////////////////////////////////

var helpArangoDatabase = arangosh.createHelpHeadline('ArangoDatabase (db) help') +
  'Administration Functions:                                                 ' + '\n' +
  '  _help()                               this help                         ' + '\n' +
  '  _flushCache()                         flush and refill collection cache ' + '\n' +
  '                                                                          ' + '\n' +
  'Collection Functions:                                                     ' + '\n' +
  '  _collections()                        list all collections              ' + '\n' +
  '  _collection(<name>)                   get collection by identifier/name ' + '\n' +
  '  _create(<name>, <properties>)         creates a new collection          ' + '\n' +
  '  _createEdgeCollection(<name>)         creates a new edge collection     ' + '\n' +
  '  _drop(<name>)                         delete a collection               ' + '\n' +
  '                                                                          ' + '\n' +
  'Document Functions:                                                       ' + '\n' +
  '  _document(<id>)                       get document by handle (_id)      ' + '\n' +
  '  _replace(<id>, <data>, <overwrite>)   overwrite document                ' + '\n' +
  '  _update(<id>, <data>, <overwrite>,    partially update document         ' + '\n' +
  '          <keepNull>)                                                     ' + '\n' +
  '  _remove(<id>)                         delete document                   ' + '\n' +
  '  _exists(<id>)                         checks whether a document exists  ' + '\n' +
  '  _truncate()                           delete all documents              ' + '\n' +
  '                                                                          ' + '\n' +
  'Database Management Functions:                                            ' + '\n' +
  '  _createDatabase(<name>)               creates a new database            ' + '\n' +
  '  _dropDatabase(<name>)                 drops an existing database        ' + '\n' +
  '  _useDatabase(<name>)                  switches into an existing database' + '\n' +
  '  _drop(<name>)                         delete a collection               ' + '\n' +
  '  _name()                               name of the current database      ' + '\n' +
  '                                                                          ' + '\n' +
  'Query / Transaction Functions:                                            ' + '\n' +
  '  _executeTransaction(<transaction>)    execute transaction               ' + '\n' +
  '  _query(<query>)                       execute AQL query                 ' + '\n' +
  '  _createStatement(<data>)              create and return AQL query       ' + '\n' +
  '                                                                          ' + '\n' +
  'View Functions:                                                           ' + '\n' +
  '  _views()                              list all views                    ' + '\n' +
  '  _view(<name>)                         get view by name                  ' + '\n' +
  '  _createView(<name>, <type>,           creates a new view                ' + '\n' +
  '              <properties>)                                               ' + '\n' +
  '  _dropView(<name>)                     delete a view                     ';

ArangoDatabase.prototype._help = function () {
  internal.print(helpArangoDatabase);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a string representation of the database object
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype.toString = function () {
  return '[object ArangoDatabase "' + this._name() + '"]';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief compacts the entire database
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._compact = function (options) {
  let url = "/_admin/compact?";
  if (options && options.changeLevel) {
    url += "changeLevel=true&";
  } 
  if (options && options.bottomMost) {
    url += "bottomMost=true";
  }
  var requestResult = this._connection.PUT(url, {});

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return all collections from the database
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._collections = function () {
  var requestResult = this._connection.GET(this._collectionurl());

  arangosh.checkRequestResult(requestResult);

  if (requestResult.result !== undefined) {
    var collections = requestResult.result;
    var result = [];
    var i;

    // add all collections to object
    for (i = 0;  i < collections.length;  ++i) {
      var collection = new ArangoCollection(this, collections[i]);
      this[collection._name] = collection;
      result.push(collection);
    }

    return result.sort(function (l, r) {
      // we assume no two collections have the same name
      if (l.name().toLowerCase() < r.name().toLowerCase()) {
        return -1;
      }
      return 1;
    });
  }

  return undefined;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a single collection, identified by its id or name
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._collection = function (id) {
  if (typeof id !== 'number' &&
      this.hasOwnProperty(id) && this[id] && this[id] instanceof ArangoCollection) {
    return this[id];
  }
  var requestResult = this._connection.GET(this._collectionurl(id));

  // return null in case of not found
  if (requestResult !== null
    && requestResult.error === true
    && requestResult.errorNum === internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code) {
    return null;
  }

  // check all other errors and throw them
  arangosh.checkRequestResult(requestResult);

  var name = requestResult.name;

  if (name !== undefined) {
    this[name] = new ArangoCollection(this, requestResult);
    return this[name];
  }

  return null;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a new collection
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._create = function (name, properties, type, options) {
  let body = {
    'name': name,
    'type': ArangoCollection.TYPE_DOCUMENT
  };

  if (properties !== undefined) {
    [ 'waitForSync', 'journalSize', 'isSystem', 'isVolatile',
      'doCompact', 'keyOptions', 'shardKeys', 'numberOfShards',
      'distributeShardsLike', 'indexBuckets', 'id', 'isSmart',
      'replicationFactor', 'minReplicationFactor', 'writeConcern', 'shardingStrategy', 'smartGraphAttribute',
      'smartJoinAttribute', 'avoidServers', 'cacheEnabled', 'schema'].forEach(function (p) {
      if (properties.hasOwnProperty(p)) {
        body[p] = properties[p];
      }
    });
  }

  if (typeof type === 'object') {
    options = type;
    type = undefined;
  }
  
  let urlAddons = [];
  if (typeof options === "object" && options !== null) {
    if (options.hasOwnProperty('waitForSyncReplication')) {
      if (options.waitForSyncReplication) {
        urlAddons.push('waitForSyncReplication=1');
      } else {
        urlAddons.push('waitForSyncReplication=0');
      }
    }
    if (options.hasOwnProperty('enforceReplicationFactor')) {
      if (options.enforceReplicationFactor) {
        urlAddons.push('enforceReplicationFactor=1');
      } else {
        urlAddons.push('enforceReplicationFactor=0');
      }
    }
  }

  let urlAddon = '';
  if (urlAddons.length > 0) {
    urlAddon += '?' + urlAddons.join('&');
  }

  if (type !== undefined) {
    body.type = type;
  }

  let requestResult = this._connection.POST(this._collectionurl() + urlAddon, body);

  arangosh.checkRequestResult(requestResult);

  name = requestResult.name;

  if (name !== undefined) {
    this[name] = new ArangoCollection(this, requestResult);
    return this[name];
  }

  return undefined;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a new document collection
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createDocumentCollection = function (name, properties) {
  return this._create(name, properties, ArangoCollection.TYPE_DOCUMENT);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a new edges collection
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createEdgeCollection = function (name, properties) {
  return this._create(name, properties, ArangoCollection.TYPE_EDGE);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief truncates a collection
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._truncate = function (id) {
  var name;

  if (typeof id !== 'string') {
    id = id._id;
  }

  for (name in this) {
    if (this.hasOwnProperty(name)) {
      var collection = this[name];

      if (collection instanceof ArangoCollection) {
        if (collection._id === id || collection._name === id) {
          return collection.truncate();
        }
      }
    }
  }

  return undefined;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief drops a collection
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._drop = function (id, options) {
  var name;

  for (name in this) {
    if (this.hasOwnProperty(name)) {
      var collection = this[name];

      if (collection instanceof ArangoCollection) {
        if (collection._id === id || collection._name === id) {
          return collection.drop(options);
        }
      }
    }
  }

  var c = this._collection(id);
  if (c) {
    return c.drop(options);
  }
  return undefined;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief flush the local cache
// / this is called by connection.reconnect()
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._flushCache = function () {
  var name;

  for (name in this) {
    if (this.hasOwnProperty(name)) {
      var collOrView = this[name];

      if (collOrView instanceof ArangoCollection ||
         collOrView instanceof ArangoView) {
        // reset the collection status
        collOrView._status = null;
        this[name] = undefined;
      }
    }
  }

  try {
    // repopulate cache
    this._collections();
  } catch (err) {}

  this._dbProperties = null;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief query the database properties
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._queryProperties = function (force) {
  if (force || this._dbProperties === null) {
    var url = '/_api/database/current';
    var requestResult = this._connection.GET(url);

    arangosh.checkRequestResult(requestResult);
    this._dbProperties = requestResult.result;
  }

  return this._dbProperties;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the database id
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._id = function () {
  return this._queryProperties().id;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return whether or not the current database is the system database
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._isSystem = function () {
  return this._queryProperties().isSystem;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the name of the current database
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._name = function () {
  return this._queryProperties().name;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the path of the current database
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._path = function () {
  return this._queryProperties().path;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns one index
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._index = function (id) {
  if (id.hasOwnProperty('id')) {
    id = id.id;
  }

  var requestResult = this._connection.GET(this._indexurl(id));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief deletes one index
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._dropIndex = function (id) {
  if (id.hasOwnProperty('id')) {
    id = id.id;
  }

  var requestResult = this._connection.DELETE(this._indexurl(id));

  if (requestResult !== null
    && requestResult.error === true
    && requestResult.errorNum === internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code) {
    return false;
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the engine name
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._engine = function () {
  var requestResult = this._connection.GET('/_api/engine');

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the engine statistics
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._engineStats = function () {
  var requestResult = this._connection.GET('/_api/engine/stats');

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the database version
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._version = function (details) {
  var requestResult = this._connection.GET('/_api/version' +
                        (details ? '?details=true' : ''));

  arangosh.checkRequestResult(requestResult);

  return details ? requestResult : requestResult.version;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a single document from the collection, identified by its id
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._document = function (id) {
  var rev = null;
  var requestResult;

  if (typeof id === 'object') {
    if (id.hasOwnProperty('_rev')) {
      rev = id._rev;
    }
    if (id.hasOwnProperty('_id')) {
      id = id._id;
    }
  }

  if (rev === null) {
    requestResult = this._connection.GET(this._documenturl(id));
  } else {
    requestResult = this._connection.GET(this._documenturl(id),
      {'if-match': JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    if (requestResult.errorNum === internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_CONFLICT.code;
    }
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks whether a document exists, identified by its id
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._exists = function (id) {
  var rev = null;
  var requestResult;

  if (typeof id === 'object') {
    if (id.hasOwnProperty('_rev')) {
      rev = id._rev;
    }
    if (id.hasOwnProperty('_id')) {
      id = id._id;
    }
  }

  if (rev === null) {
    requestResult = this._connection.HEAD(this._documenturl(id));
  } else {
    requestResult = this._connection.HEAD(this._documenturl(id),
      {'if-match': JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    if (requestResult.errorNum === internal.errors.ERROR_HTTP_NOT_FOUND.code) {
      return false;
    }
    if (requestResult.errorNum === internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_CONFLICT.code;
    }
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief delete a document in the collection, identified by its id
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._remove = function (id, overwrite, waitForSync) {
  var rev = null;
  var requestResult;

  if (typeof id === 'object') {
    if (Array.isArray(id)) {
      throw new ArangoError({
        error: true,
        code: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
        errorNum: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
        errorMessage: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
      });
    }
    if (id.hasOwnProperty('_rev')) {
      rev = id._rev;
    }
    if (id.hasOwnProperty('_id')) {
      id = id._id;
    }
  }

  var params = '';
  var ignoreRevs = false;
  var options;

  if (typeof overwrite === 'object') {
    if (typeof waitForSync !== 'undefined') {
      throw 'too many arguments';
    }
    // we assume the caller uses new signature (id, data, options)
    options = overwrite;
    if (options.hasOwnProperty('overwrite') && options.overwrite) {
      ignoreRevs = true;
    }
    if (options.hasOwnProperty('waitForSync')) {
      waitForSync = options.waitForSync;
    }
  } else {
    if (overwrite) {
      ignoreRevs = true;
    }
    options = {};
  }

  var url = this._documenturl(id) + params;
  url = appendSyncParameter(url, waitForSync);
  url = appendBoolParameter(url, 'ignoreRevs', ignoreRevs);
  url = appendBoolParameter(url, 'returnOld', options.returnOld);

  if (rev === null || ignoreRevs) {
    requestResult = this._connection.DELETE(url);
  } else {
    requestResult = this._connection.DELETE(url, null,
      {'if-match': JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    if (requestResult.errorNum === internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_CONFLICT.code;
    }
  }

  arangosh.checkRequestResult(requestResult);

  return options.silent ? true : requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief replace a document in the collection, identified by its id
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._replace = function (id, data, overwrite, waitForSync) {
  var rev = null;
  var requestResult;

  if (typeof id === 'object') {
    if (Array.isArray(id)) {
      throw new ArangoError({
        error: true,
        code: internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
        errorNum: internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
        errorMessage: internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.message
      });
    }
    if (id.hasOwnProperty('_rev')) {
      rev = id._rev;
    }
    if (id.hasOwnProperty('_id')) {
      id = id._id;
    }
  }

  var params = '';
  var ignoreRevs = false;
  var options;

  if (typeof overwrite === 'object') {
    if (typeof waitForSync !== 'undefined') {
      throw 'too many arguments';
    }
    // we assume the caller uses new signature (id, data, options)
    options = overwrite;
    if (options.hasOwnProperty('overwrite') && options.overwrite) {
      ignoreRevs = true;
    }
    if (options.hasOwnProperty('waitForSync')) {
      waitForSync = options.waitForSync;
    }
  } else {
    if (overwrite) {
      ignoreRevs = true;
    }
    options = {};
  }
  var url = this._documenturl(id) + params;
  url = appendSyncParameter(url, waitForSync);
  url = appendBoolParameter(url, 'ignoreRevs', true);
  url = appendBoolParameter(url, 'returnOld', options.returnOld);
  url = appendBoolParameter(url, 'returnNew', options.returnNew);

  if (rev === null || ignoreRevs) {
    requestResult = this._connection.PUT(url, data);
  } else {
    requestResult = this._connection.PUT(url, data,
      {'if-match': JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    if (requestResult.errorNum === internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_CONFLICT.code;
    }
  }

  arangosh.checkRequestResult(requestResult);

  return options.silent ? true : requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief update a document in the collection, identified by its id
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._update = function (id, data, overwrite, keepNull, waitForSync) {
  var rev = null;
  var requestResult;

  if (typeof id === 'object') {
    if (Array.isArray(id)) {
      throw new ArangoError({
        error: true,
        code: internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
        errorNum: internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
        errorMessage: internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.message
      });
    }
    if (id.hasOwnProperty('_rev')) {
      rev = id._rev;
    }
    if (id.hasOwnProperty('_id')) {
      id = id._id;
    }
  }

  var params = '';
  var ignoreRevs = false;
  var options;
  if (typeof overwrite === 'object') {
    if (typeof keepNull !== 'undefined') {
      throw 'too many arguments';
    }
    // we assume the caller uses new signature (id, data, options)
    options = overwrite;
    if (!options.hasOwnProperty('keepNull')) {
      options.keepNull = true;
    }
    params = '?keepNull=' + options.keepNull;
    if (!options.hasOwnProperty('mergeObjects')) {
      options.mergeObjects = true;
    }
    params += '&mergeObjects=' + options.mergeObjects;

    if (options.hasOwnProperty('overwrite') && options.overwrite) {
      ignoreRevs = true;
    }
  } else {
    // set default value for keepNull
    var keepNullValue = ((typeof keepNull === 'undefined') ? true : keepNull);
    params = '?keepNull=' + (keepNullValue ? 'true' : 'false');

    if (overwrite) {
      ignoreRevs = true;
    }
    options = {};
  }
  var url = this._documenturl(id) + params;
  url = appendSyncParameter(url, waitForSync);
  url = appendBoolParameter(url, 'ignoreRevs', true);
  url = appendBoolParameter(url, 'returnOld', options.returnOld);
  url = appendBoolParameter(url, 'returnNew', options.returnNew);

  if (rev === null || ignoreRevs) {
    requestResult = this._connection.PATCH(url, data);
  } else {
    requestResult = this._connection.PATCH(url, data,
      {'if-match': JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    if (requestResult.errorNum === internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code) {
      requestResult.errorNum = internal.errors.ERROR_ARANGO_CONFLICT.code;
    }
  }

  arangosh.checkRequestResult(requestResult);

  return options.silent ? true : requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief factory method to create a new statement
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createStatement = function (data) {
  return new ArangoStatement(this, data);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief factory method to create and execute a new statement
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._query = function (query, bindVars, cursorOptions, options) {
  var payload = {
    query,
    bindVars: bindVars || undefined
  };

  if (query && typeof query === 'object' && typeof query.toAQL !== 'function') {
    payload = query;
    options = cursorOptions;
    cursorOptions = bindVars;
  }
  if (options === undefined && cursorOptions !== undefined) {
    options = cursorOptions;
  }

  if (options) {
    payload.options = options || undefined;
    payload.cache = (options && options.cache) || undefined;
  }
  if (cursorOptions) {
    payload.count = (cursorOptions && cursorOptions.count) || false;
    payload.batchSize = (cursorOptions && cursorOptions.batchSize) || undefined;
  }

  return new ArangoStatement(this, payload).execute();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief queryProfile execute a query with profiling information
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._profileQuery = function (query, bindVars, options) {
  options = options || {};
  options.profile = 2;
  query = { query: query, bindVars: bindVars, options: options };
  require('@arangodb/aql/explainer').profileQuery(query);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief explains a query
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._explain = function (query, bindVars, options) {
  if (typeof query === 'object' && typeof query.toAQL === 'function') {
    query = { query: query.toAQL() };
  }

  if (arguments.length > 1) {
    query = { query: query, bindVars: bindVars, options: options };
  }

  require('@arangodb/aql/explainer').explain(query);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief parses a query
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._parse = function (query) {
  if (typeof query === 'object' && typeof query.toAQL === 'function') {
    query = { query: query.toAQL() };
  } else {
    query = { query };
  }

  const requestResult = this._connection.POST('/_api/query', query);

  if (requestResult && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief create a new database
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createDatabase = function (name, options, users) {
  var data = {
    name: name,
    options: options || { },
    users: users || []
  };

  var requestResult = this._connection.POST('/_api/database', data);

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief drop an existing database
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._dropDatabase = function (name) {
  var requestResult = this._connection.DELETE('/_api/database/' + encodeURIComponent(name));

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief list all existing databases
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._databases = function () {
  var requestResult = this._connection.GET('/_api/database');

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief show properties
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._properties = function () {
  return this._queryProperties(true);
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief uses a database
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._useDatabase = function (name) {
  var old = this._connection.getDatabaseName();

  // no change
  if (name === old) {
    return true;
  }

  this._connection.setDatabaseName(name);

  try {
    // re-query properties
    this._queryProperties(true);
    this._flushCache();
  } catch (err) {
    this._connection.setDatabaseName(old);

    if (err.hasOwnProperty('errorNum')) {
      throw err;
    }

    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: "cannot use database '" + name + "'"
    });
  }

  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief lists all endpoints
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._endpoints = function () {
  var requestResult = this._connection.GET('/_api/endpoint');

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief execute a transaction
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._executeTransaction = function (data) {
  if (!data || typeof (data) !== 'object') {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: 'usage: _executeTransaction(<object>)'
    });
  }

  data = Object.assign({}, data);

  if (!data.collections || typeof data.collections !== 'object') {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: 'missing/invalid collections definition for transaction'
    });
  }

  data.collections = Object.assign({}, data.collections);
  if (data.collections.read) {
    if (!Array.isArray(data.collections.read)) {
      data.collections.read = [data.collections.read];
    }
    data.collections.read = data.collections.read.map(
      col => col.isArangoCollection ? col.name() : col
    );
  }
  if (data.collections.write) {
    if (!Array.isArray(data.collections.write)) {
      data.collections.write = [data.collections.write];
    }
    data.collections.write = data.collections.write.map(
      col => col.isArangoCollection ? col.name() : col
    );
  }

  if (!data.action ||
    (typeof (data.action) !== 'string' && typeof (data.action) !== 'function')) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: 'missing/invalid action definition for transaction'
    });
  }

  if (typeof (data.action) === 'function') {
    data.action = String(data.action);
  }

  var requestResult = this._connection.POST('/_api/transaction', data);

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief create a transaction
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createTransaction = function (data) {
  return new ArangoTransaction(this, data);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the currently ongoing managed transactions
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._transactions = function () {
  var requestResult = this._connection.GET("/_api/transaction");

  arangosh.checkRequestResult(requestResult);
  return requestResult.transactions;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a new view
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createView = function (name, type, properties) {
  let body = properties === undefined ? {} : properties;

  if (name === undefined) {
    delete body['name'];
  } else {
    body['name'] = name;
  }

  if (type === undefined) {
    delete body['type'];
  } else {
    body['type'] = type;
  }

  let requestResult = this._connection.POST(this._viewurl(), body);

  arangosh.checkRequestResult(requestResult);

  name = requestResult.name;

  if (name !== undefined) {
    this[name] = new ArangoView(this, requestResult);
    return this[name];
  }

  return undefined;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief deletes a view
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._dropView = function (id) {
  var name;

  for (name in this) {
    if (this.hasOwnProperty(name)) {
      var view = this[name];

      if (view instanceof ArangoView) {
        if (view._id === id || view._name === id) {
          return view.drop();
        }
      }
    }
  }

  var v = this._view(id);

  if (v) {
    return v.drop();
  }

  return undefined;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return all views from the database
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._views = function () {
  var requestResult = this._connection.GET(this._viewurl());

  arangosh.checkRequestResult(requestResult);

  var result = [];
  if (requestResult !== undefined) {
    var views = requestResult.result;
    var i;

    // add all views to object
    for (i = 0;  i < views.length;  ++i) {
      var view = new ArangoView(this, views[i]);
      this[view._name] = view;
      result.push(view);
    }

    result = result.sort(function (l, r) {
      // we assume no two views have the same name
      if (l.name().toLowerCase() < r.name().toLowerCase()) {
        return -1;
      }
      return 1;
    });
  }

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a single view, identified by its name
// //////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._view = function (id) {
  if (typeof id !== 'number'
      && this.hasOwnProperty(id)
      && this[id]
      && this[id] instanceof ArangoView) {
    return this[id];
  }

  var url = this._viewurl(id);
  var requestResult = this._connection.GET(url);

  // return null in case of not found
  if (requestResult !== null
    && requestResult.error === true
    && requestResult.errorNum === internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code) {
    return null;
  }

  // check all other errors and throw them
  arangosh.checkRequestResult(requestResult);

  var name = requestResult.name;

  if (name !== undefined) {
    this[name] = new ArangoView(this, requestResult);
    return this[name];
  }

  return null;
};

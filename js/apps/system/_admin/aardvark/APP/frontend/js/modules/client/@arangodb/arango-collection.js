module.define("@arangodb/arango-collection", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoCollection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangosh = require("@arangodb/arangosh");


////////////////////////////////////////////////////////////////////////////////
/// @brief add options from arguments to index specification
////////////////////////////////////////////////////////////////////////////////

function addIndexOptions (body, parameters) {
  body.fields = [ ];

  var setOption = function(k) {
    if (! body.hasOwnProperty(k)) {
      body[k] = parameters[i][k];
    }
  };

  var i;
  for (i = 0; i < parameters.length; ++i) {
    if (typeof parameters[i] === "string") {
      // set fields
      body.fields.push(parameters[i]);
    }
    else if (typeof parameters[i] === "object" && 
             ! Array.isArray(parameters[i]) &&
             parameters[i] !== null) {
      // set arbitrary options
      Object.keys(parameters[i]).forEach(setOption);
      break;
    }
  }

  return body;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function ArangoCollection (database, data) {
  this._database = database;
  this._dbName = database._name();

  if (typeof data === "string") {
    this._id = null;
    this._name = data;
    this._status = null;
    this._type = null;
  }
  else if (data !== undefined) {
    this._id = data.id;
    this._name = data.name;
    this._status = data.status;
    this._type = data.type;
  }
  else {
    this._id = null;
    this._name = null;
    this._status = null;
    this._type = null;
  }
}

exports.ArangoCollection = ArangoCollection;

// must be called after exporting ArangoCollection
require("@arangodb/arango-collection-common");

var ArangoError = require("@arangodb").ArangoError;


////////////////////////////////////////////////////////////////////////////////
/// @brief append the waitForSync parameter to a URL
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._appendSyncParameter = function (url, waitForSync) {
  if (waitForSync) {
    if (url.indexOf('?') === -1) {
      url += '?';
    }
    else {
      url += '&';
    }
    url += 'waitForSync=true';
  }
  return url;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prefix a URL with the database name of the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._prefixurl = function (url) {
  if (url.substr(0, 5) === '/_db/') {
    return url;
  }

  if (url[0] === '/') {
    return '/_db/' + encodeURIComponent(this._dbName) + url;
  }
  return '/_db/' + encodeURIComponent(this._dbName) + '/' + url;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for collection usage
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._baseurl = function (suffix) {
  var url = this._database._collectionurl(this.name());

  if (suffix) {
    url += "/" + suffix;
  }

  return this._prefixurl(url);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for document usage
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._documenturl = function (id) {
  var s = id.split("/"), url;
  if (s.length === 1) {
    url = this._database._documenturl(this.name() + "/" + id, this.name());
  }
  else {
    url = this._database._documenturl(id, this.name());
  }

  return this._prefixurl(url);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for collection index usage
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._indexurl = function () {
  return this._prefixurl("/_api/index?collection=" + encodeURIComponent(this.name()));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an edge query
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._edgesQuery = function (vertex, direction) {
  // if vertex is a list, iterator and concat
  if (vertex instanceof Array) {
    var edges = [];
    var i;

    for (i = 0;  i < vertex.length;  ++i) {
      var e = this._edgesQuery(vertex[i], direction);

      edges.push.apply(edges, e);
    }

    return edges;
  }

  if (vertex.hasOwnProperty("_id")) {
    vertex = vertex._id;
  }

  // get the edges
  var url = "/_api/edges/" + encodeURIComponent(this.name())
            + "?vertex=" + encodeURIComponent(vertex)
            + (direction ? "&direction=" + direction : "");

  var requestResult = this._database._connection.GET(this._prefixurl(url));

  arangosh.checkRequestResult(requestResult);

  return requestResult.edges;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief converts into an array
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toArray = function () {
  return this.all().toArray();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for ArangoCollection
////////////////////////////////////////////////////////////////////////////////

var helpArangoCollection = arangosh.createHelpHeadline("ArangoCollection help") +
  'ArangoCollection constructor:                                             ' + "\n" +
  ' > col = db.mycoll;                                                       ' + "\n" +
  ' > col = db._create("mycoll");                                            ' + "\n" +
  '                                                                          ' + "\n" +
  'Administration Functions:                                                 ' + "\n" +
  '  name()                                collection name                   ' + "\n" +
  '  status()                              status of the collection          ' + "\n" +
  '  type()                                type of the collection            ' + "\n" +
  '  truncate()                            delete all documents              ' + "\n" +
  '  properties()                          show collection properties        ' + "\n" +
  '  drop()                                delete a collection               ' + "\n" +
  '  load()                                load a collection                 ' + "\n" +
  '  unload()                              unload a collection               ' + "\n" +
  '  rename(<new-name>)                    renames a collection              ' + "\n" +
  '  getIndexes()                          return defined indexes            ' + "\n" +
  '  refresh()                             refreshes the status and name     ' + "\n" +
  '  _help()                               this help                         ' + "\n" +
  '                                                                          ' + "\n" +
  'Document Functions:                                                       ' + "\n" +
  '  count()                               return number of documents        ' + "\n" +
  '  save(<data>)                          create document and return handle ' + "\n" +
  '  document(<id>)                        get document by handle (_id or _key)' + "\n" +
  '  replace(<id>, <data>, <overwrite>)    overwrite document                ' + "\n" +
  '  update(<id>, <data>, <overwrite>,     partially update document         ' + "\n" +
  '         <keepNull>)                                                      ' + "\n" +
  '  remove(<id>)                          delete document                   ' + "\n" +
  '  exists(<id>)                          checks whether a document exists  ' + "\n" +
  '  first()                               first inserted/updated document   ' + "\n" +
  '  last()                                last inserted/updated document    ' + "\n" +
  '                                                                          ' + "\n" +
  'Attributes:                                                               ' + "\n" +
  '  _database                             database object                   ' + "\n" +
  '  _id                                   collection identifier             ';

ArangoCollection.prototype._help = function () {
  internal.print(helpArangoCollection);
};


////////////////////////////////////////////////////////////////////////////////
/// @brief gets the name of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.name = function () {
  if (this._name === null) {
    this.refresh();
  }

  return this._name;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the status of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.status = function () {
  var result;

  if (this._status === null) {
    this.refresh();
  }

  // save original status
  result = this._status;

  if (this._status === ArangoCollection.STATUS_UNLOADING) {
    // if collection is currently unloading, we must not cache this info
    this._status = null;
  }

  // return the correct result
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the type of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.type = function () {
  if (this._type === null) {
    this.refresh();
  }

  return this._type;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the properties of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.properties = function (properties) {
  var attributes = {
    "doCompact": true,
    "journalSize": true,
    "isSystem": false,
    "isVolatile": false,
    "waitForSync": true,
    "shardKeys": false,
    "numberOfShards": false,
    "keyOptions": false,
    "indexBuckets": true
  };
  var a;

  var requestResult;
  if (properties === undefined) {
    requestResult = this._database._connection.GET(this._baseurl("properties"));

    arangosh.checkRequestResult(requestResult);
  }
  else {
    var body = {};

    for (a in attributes) {
      if (attributes.hasOwnProperty(a) &&
          attributes[a] &&
          properties.hasOwnProperty(a)) {
        body[a] = properties[a];
      }
    }

    requestResult = this._database._connection.PUT(this._baseurl("properties"),
      JSON.stringify(body));

    arangosh.checkRequestResult(requestResult);
  }

  var result = { };
  for (a in attributes) {
    if (attributes.hasOwnProperty(a) &&
        requestResult.hasOwnProperty(a) &&
        requestResult[a] !== undefined) {
      result[a] = requestResult[a];
    }
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the journal of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.rotate = function () {
  var requestResult = this._database._connection.PUT(this._baseurl("rotate"), "");

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the figures of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.figures = function () {
  var requestResult = this._database._connection.GET(this._baseurl("figures"));

  arangosh.checkRequestResult(requestResult);

  return requestResult.figures;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the checksum of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.checksum = function (withRevisions, withData) {
  var append = '';
  if (withRevisions) {
    append += '?withRevisions=true';
  }
  if (withData) {
    append += (append === '' ? '?' : '&') + 'withData=true';
  }
  var requestResult = this._database._connection.GET(this._baseurl("checksum") + append);

  arangosh.checkRequestResult(requestResult);

  return {
    checksum: requestResult.checksum,
    revision: requestResult.revision
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the revision id of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.revision = function () {
  var requestResult = this._database._connection.GET(this._baseurl("revision"));

  arangosh.checkRequestResult(requestResult);

  return requestResult.revision;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.drop = function () {
  var requestResult = this._database._connection.DELETE(this._baseurl());

  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum !== internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
      // check error in case we got anything else but "collection not found"
     arangosh.checkRequestResult(requestResult);
  }

  this._status = ArangoCollection.STATUS_DELETED;

  var database = this._database;
  var name;

  for (name in database) {
    if (database.hasOwnProperty(name)) {
      var collection = database[name];

      if (collection instanceof ArangoCollection) {
        if (collection.name() === this.name()) {
          delete database[name];
        }
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.truncate = function () {
  var requestResult = this._database._connection.PUT(this._baseurl("truncate"), "");

  arangosh.checkRequestResult(requestResult);

  this._status = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.load = function (count) {
  var data = { count: true };

  // return the number of documents? this might slow down loading
  if (count !== undefined) {
    data.count = count;
  }

  var requestResult = this._database._connection.PUT(this._baseurl("load"), JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  this._status = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.unload = function () {
  var requestResult = this._database._connection.PUT(this._baseurl("unload"), "");

  arangosh.checkRequestResult(requestResult);

  this._status = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.rename = function (name) {
  var body = { name : name };
  var requestResult = this._database._connection.PUT(this._baseurl("rename"), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  delete this._database[this._name];
  this._database[name] = this;

  this._status = null;
  this._name = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief refreshes a collection status and name
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.refresh = function () {
  var requestResult = this._database._connection.GET(
    this._database._collectionurl(this._id) + "?useId=true");

  arangosh.checkRequestResult(requestResult);

  this._name = requestResult.name;
  this._status = requestResult.status;
  this._type = requestResult.type;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief gets all indexes
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.getIndexes = function (withStats) {
  var requestResult = this._database._connection.GET(this._indexurl()+
          "&withStats="+(withStats || false));

  arangosh.checkRequestResult(requestResult);

  return requestResult.indexes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.index = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var requestResult = this._database._connection.GET(this._database._indexurl(id, this.name()));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.dropIndex = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var requestResult = this._database._connection.DELETE(this._database._indexurl(id, this.name()));

  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum
      === internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code) {
    return false;
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a cap constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureCapConstraint = function (size, byteSize) {
  var body = {
    type : "cap",
    size : size || undefined,
    byteSize: byteSize || undefined
  };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a unique skip-list index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueSkiplist = function () {
  var body = addIndexOptions({
    type : "skiplist",
    unique : true
  }, arguments);

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a skip-list index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureSkiplist = function () {
  var body = addIndexOptions({
    type : "skiplist",
    unique : false
  }, arguments);

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a fulltext index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureFulltextIndex = function (field, minLength) {
  var body = {
    type: "fulltext",
    minLength: minLength || undefined,
    fields: [ field ]
  };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a unique constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueConstraint = function () {
  var body = addIndexOptions({
    type : "hash",
    unique : true
  }, arguments);

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a hash index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureHashIndex = function () {
  var body = addIndexOptions({
    type : "hash",
    unique : false
  }, arguments);

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a geo index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoIndex = function (lat, lon) {
  var body;

  if (typeof lat !== "string") {
    throw "usage: ensureGeoIndex(<lat>, <lon>) or ensureGeoIndex(<loc>[, <geoJson>])";
  }

  if (typeof lon === "boolean") {
    body = {
      type : "geo",
      fields : [ lat ],
      geoJson : lon
    };
  }
  else if (lon === undefined) {
    body = {
      type : "geo",
      fields : [ lat ],
      geoJson : false
    };
  }
  else {
    body = {
      type : "geo",
      fields : [ lat, lon ],
      geoJson: false
    };
  }

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a geo constraint
/// since ArangoDB 2.5, this is just a redirection to ensureGeoIndex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoConstraint = function (lat, lon) {
  return this.ensureGeoIndex(lat, lon);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures an index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureIndex = function (data) {
  if (typeof data !== "object" || Array.isArray(data)) {
    throw "usage: ensureIndex(<description>)";
  }

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief gets the number of documents
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.count = function () {
  var requestResult = this._database._connection.GET(this._baseurl("count"));

  arangosh.checkRequestResult(requestResult);

  return requestResult.count;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a single document from the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.document = function (id) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev === null) {
    requestResult = this._database._connection.GET(this._documenturl(id));
  }
  else {
    requestResult = this._database._connection.GET(this._documenturl(id),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a specific document exists
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.exists = function (id) {
  var rev = null;
  var requestResult;

  if (id === undefined || id === null) {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev === null) {
    requestResult = this._database._connection.HEAD(this._documenturl(id));
  }
  else {
    requestResult = this._database._connection.HEAD(this._documenturl(id),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null &&
      requestResult.error === true &&
      (requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code ||
       requestResult.errorNum === internal.errors.ERROR_HTTP_NOT_FOUND.code ||
       requestResult.errorNum === internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code)) {
    return false;
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a random element from the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.any = function () {
  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/any"),
    JSON.stringify({ collection: this._name }));

  arangosh.checkRequestResult(requestResult);

  return requestResult.document;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example for a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.firstExample = function (example) {
  var e;
  var i;

  // example is given as only argument
  if (arguments.length === 1) {
    e = example;
  }

  // example is given as list
  else {
    e = {};

    for (i = 0;  i < arguments.length;  i += 2) {
      e[arguments[i]] = arguments[i + 1];
    }
  }

  var data = {
    collection: this.name(),
    example: e
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/first-example"),
    JSON.stringify(data)
  );

  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum === internal.errors.ERROR_HTTP_NOT_FOUND.code) {
    return null;
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.document;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the first document(s) from the collection
/// If an argument is supplied, the result will be a list of the first n
/// documents. When no argument is supplied, the result is the first document
/// from the collection, or null if the collection is empty.
/// The document order is determined by the insertion/update order.
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.first = function (count) {
  var body = {
    collection: this.name(),
    count: count
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/first"),
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last document(s) from the collection
/// If an argument is supplied, the result will be a list of the last n
/// documents. When no argument is supplied, the result is the last document
/// from the collection, or null if the collection is empty.
/// The document order is determined by the insertion/update order.
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.last = function (count) {
  var body = {
    collection: this.name(),
    count: count
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/last"),
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a document in the collection
/// note: this method is used to save documents and edges, but save() has a
/// different signature for both. For document collections, the signature is
/// save(<data>, <waitForSync>), whereas for edge collections, the signature is
/// save(<from>, <to>, <data>, <waitForSync>)
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.save =
ArangoCollection.prototype.insert = function (from, to, data, waitForSync) {
  var type = this.type(), url;

  if (type === undefined) {
    type = ArangoCollection.TYPE_DOCUMENT;
  }

  if (type === ArangoCollection.TYPE_DOCUMENT) {
    data = from;
    waitForSync = to;
    url = "/_api/document?collection=" + encodeURIComponent(this.name());
  }
  else if (type === ArangoCollection.TYPE_EDGE) {
    if (typeof from === 'object' && from.hasOwnProperty("_id")) {
      from = from._id;
    }

    if (typeof to === 'object' && to.hasOwnProperty("_id")) {
      to = to._id;
    }

    url = "/_api/edge?collection=" + encodeURIComponent(this.name())
        + "&from=" + encodeURIComponent(from)
        + "&to=" + encodeURIComponent(to);
  }

  url = this._appendSyncParameter(url, waitForSync);

  if (data === undefined || typeof data !== 'object') {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.message
    });
  }

  var requestResult = this._database._connection.POST(
    this._prefixurl(url),
    JSON.stringify(data)
  );

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document in the collection
/// @param id the id of the document
/// @param overwrite (optional) a boolean value or a json object
/// @param waitForSync (optional) a boolean value .
/// @example remove("example/996280832675")
/// @example remove("example/996280832675", true)
/// @example remove("example/996280832675", false)
/// @example remove("example/996280832675", {waitForSync: false, overwrite: true})
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.remove = function (id, overwrite, waitForSync) {
  var rev = null;
  var requestResult;

  if (id === undefined || id === null) {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var params = "";

  if (typeof overwrite === "object") {
    // we assume the caller uses new signature (id, data, options)
    if (typeof waitForSync !== "undefined") {
      throw "too many arguments";
    }
    var options = overwrite;
    if (options.hasOwnProperty("overwrite") && options.overwrite) {
      params += "?policy=last";
    }
    if (options.hasOwnProperty("waitForSync") ) {
     waitForSync = options.waitForSync;
    }
  } else {
    if (overwrite) {
      params += "?policy=last";
    }
  }

  var url = this._documenturl(id) + params;
  url = this._appendSyncParameter(url, waitForSync);

  if (rev === null) {
    requestResult = this._database._connection.DELETE(url);
  }
  else {
    requestResult = this._database._connection.DELETE(url,
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    if (overwrite) {
      if (requestResult.errorNum === internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        return false;
      }
    }

    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document in the collection
/// @param id the id of the document
/// @param overwrite (optional) a boolean value or a json object
/// @param waitForSync (optional) a boolean value .
/// @example replace("example/996280832675", { a : 1, c : 2} )
/// @example replace("example/996280832675", { a : 1, c : 2}, true)
/// @example replace("example/996280832675", { a : 1, c : 2}, false)
/// @example replace("example/996280832675", { a : 1, c : 2}, {waitForSync: false, overwrite: true})
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replace = function (id, data, overwrite, waitForSync) {
  var rev = null;
  var requestResult;

  if (id === undefined || id === null) {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var params = "";
  if (typeof overwrite === "object") {
    if (typeof waitForSync !== "undefined") {
      throw "too many arguments";
    }
    // we assume the caller uses new signature (id, data, options)
    var options = overwrite;
    if (options.hasOwnProperty("overwrite") && options.overwrite) {
      params += "?policy=last";
    }
    if (options.hasOwnProperty("waitForSync") ) {
     waitForSync = options.waitForSync;
    }
  } else {
    if (overwrite) {
      params += "?policy=last";
    }
  }
  var url = this._documenturl(id) + params;
  url = this._appendSyncParameter(url, waitForSync);
  if (rev === null) {
    requestResult = this._database._connection.PUT(url, JSON.stringify(data));
  }
  else {
    requestResult = this._database._connection.PUT(url, JSON.stringify(data),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document in the collection
/// @param id the id of the document
/// @param overwrite (optional) a boolean value or a json object
/// @param keepNull (optional) determines if null values should saved or not
/// @param mergeObjects (optional) whether or not object values should be merged
/// @param waitForSync (optional) a boolean value .
/// @example update("example/996280832675", { a : 1, c : 2} )
/// @example update("example/996280832675", { a : 1, c : 2, x: null}, true, true, true)
/// @example update("example/996280832675", { a : 1, c : 2, x: null},
//                 {keepNull: true, waitForSync: false, overwrite: true})
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.update = function (id, data, overwrite, keepNull, waitForSync) {
  var rev = null;
  var requestResult;

  if (id === undefined || id === null) {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var params = "";
  if (typeof overwrite === "object") {
    if (typeof keepNull !== "undefined") {
      throw "too many arguments";
    }
    // we assume the caller uses new signature (id, data, options)
    var options = overwrite;
    if (! options.hasOwnProperty("keepNull")) {
      options.keepNull = true;
    }
    params = "?keepNull=" + options.keepNull;

    if (! options.hasOwnProperty("mergeObjects")) {
      options.mergeObjects = true;
    }
    params += "&mergeObjects=" + options.mergeObjects;

    if (options.hasOwnProperty("overwrite") && options.overwrite) {
      params += "&policy=last";
    }

  } else {

    // set default value for keepNull
    var keepNullValue = ((typeof keepNull === "undefined") ? true : keepNull);
    params = "?keepNull=" + (keepNullValue ? "true" : "false");

    if (overwrite) {
      params += "&policy=last";
    }

  }

  var url = this._documenturl(id) + params;
  url = this._appendSyncParameter(url, waitForSync);

  if (rev === null) {
    requestResult = this._database._connection.PATCH(url, JSON.stringify(data));
  }
  else {
    requestResult = this._database._connection.PATCH(url, JSON.stringify(data),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting or ending in a vertex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.edges = function (vertex) {
  return this._edgesQuery(vertex);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges ending in a vertex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.inEdges = function (vertex) {
  return this._edgesQuery(vertex, "in");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting in a vertex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.outEdges = function (vertex) {
  return this._edgesQuery(vertex, "out");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes documents matching an example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByExample = function (example,
                                                       waitForSync,
                                                       limit) {
  var data = {
    collection: this._name,
    example: example,
    waitForSync: waitForSync,
    limit: limit
  };

  if (typeof waitForSync === "object") {
    if (typeof limit !== "undefined") {
      throw "too many parameters";
    }
    data = {
      collection: this._name,
      example: example,
      options: waitForSync
    };
  }

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/remove-by-example"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return requestResult.deleted;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces documents matching an example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replaceByExample = function (example,
                                                        newValue,
                                                        waitForSync,
                                                        limit) {
  var data = {
    collection: this._name,
    example: example,
    newValue: newValue,
    waitForSync: waitForSync,
    limit: limit
  };

  if (typeof waitForSync === "object") {
    if (typeof limit !== "undefined") {
      throw "too many parameters";
    }
    data = {
      collection: this._name,
      example: example,
      newValue: newValue,
      options: waitForSync
    };
  }
  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/replace-by-example"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return requestResult.replaced;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief updates documents matching an example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.updateByExample = function (example,
                                                       newValue,
                                                       keepNull,
                                                       waitForSync,
                                                       limit) {
  var data = {
    collection: this._name,
    example: example,
    newValue: newValue,
    keepNull: keepNull,
    waitForSync: waitForSync,
    limit: limit
  };
  if (typeof keepNull === "object") {
    if (typeof waitForSync !== "undefined") {
      throw "too many parameters";
    }
    var options = keepNull;
    data = {
      collection: this._name,
      example: example,
      newValue: newValue,
      options: options
    };
  }
  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/update-by-example"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return requestResult.updated;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents by keys
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.documents = function (keys) {
  var data = {
    collection: this._name,
    keys: keys || [ ]
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/lookup-by-keys"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return {
    documents: requestResult.documents
  };
};

// .lookupByKeys is now an alias for .documents
ArangoCollection.prototype.lookupByKeys = ArangoCollection.prototype.documents;

////////////////////////////////////////////////////////////////////////////////
/// @brief removes documents by keys
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByKeys = function (keys) {
  var data = {
    collection: this._name,
    keys: keys || [ ]
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/remove-by-keys"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return {
    removed: requestResult.removed,
    ignored: requestResult.ignored
  };
};


});

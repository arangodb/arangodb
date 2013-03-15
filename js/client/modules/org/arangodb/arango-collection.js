/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, stupid: true */
/*global require, exports */

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
var arangosh = require("org/arangodb/arangosh");

var ArangoError = require("org/arangodb/arango-error").ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoCollection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function ArangoCollection (database, data) {
  this._database = database;

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
require("org/arangodb/arango-collection-common");

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

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
/// @brief return the base url for collection usage
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._baseurl = function (suffix) {
  var url = this._database._collectionurl(this.name());

  if (suffix) {
    url += "/" + suffix;
  }
    
  return url;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for document usage
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._documenturl = function (id) {
  var s = id.split("/");

  if (s.length === 1) {
    return this._database._documenturl(this.name() + "/" + id, this.name()); 
  }

  return this._database._documenturl(id, this.name()); 
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for collection index usage
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._indexurl = function (suffix) {
  return "/_api/index?collection=" + encodeURIComponent(this.name());
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
  var requestResult = this._database._connection.GET(
    "/_api/edges/" + encodeURIComponent(this.name()) 
    + "?vertex=" + encodeURIComponent(vertex) 
    + (direction ? "&direction=" + direction : ""));

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
/// @brief queries by example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.BY_EXAMPLE_HASH = function (index, example, skip, limit) {
  var key;
  var body;

  limit = limit || null;
  skip = skip || null;

  if (index.hasOwnProperty("id")) {
    index = index.id;
  }

  body = {
    collection : this.name(),
    index : index,
    skip : skip,
    limit : limit,
    example : {} 
  };

  for (key in example) {
    if (example.hasOwnProperty(key)) {
      body.example[key] = example[key];
    }
  }

  var requestResult = this._database._connection.PUT(
    "/_api/simple/BY-EXAMPLE-HASH",
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for ArangoCollection
////////////////////////////////////////////////////////////////////////////////

var helpArangoCollection = arangosh.createHelpHeadline("ArangoCollection help") +
  'ArangoCollection constructor:                                       ' + "\n" +
  ' > col = db.mycoll;                                                 ' + "\n" +
  ' > col = db._create("mycoll");                                      ' + "\n" +
  '                                                                    ' + "\n" +
  'Administration Functions:                                           ' + "\n" +
  '  name()                          collection name                   ' + "\n" +
  '  status()                        status of the collection          ' + "\n" +
  '  type()                          type of the collection            ' + "\n" +
  '  truncate()                      delete all documents              ' + "\n" +
  '  properties()                    show collection properties        ' + "\n" +
  '  drop()                          delete a collection               ' + "\n" +
  '  load()                          load a collection into memeory    ' + "\n" +
  '  unload()                        unload a collection from memory   ' + "\n" +
  '  rename(new-name)                renames a collection              ' + "\n" +
  '  refresh()                       refreshes the status and name     ' + "\n" +
  '  _help();                        this help                         ' + "\n" +
  '                                                                    ' + "\n" +
  'Document Functions:                                                 ' + "\n" +
  '  count()                         number of documents               ' + "\n" +
  '  save(<data>)                    create document and return handle ' + "\n" +
  '  document(<id>)                  get document by handle            ' + "\n" +
  '  replace(<id>, <data>,           overwrite document                ' + "\n" +
  '          <overwrite>)                                              ' + "\n" +
  '  update(<id>, <data>,            partially update document         ' + "\n" +
  '         <overwrite>, <keepNull>)                                   ' + "\n" +
  '  delete(<id>)                    delete document                   ' + "\n" +
  '                                                                    ' + "\n" +
  'Attributes:                                                         ' + "\n" +
  '  _database                       database object                   ' + "\n" +
  '  _id                             collection identifier             ';

ArangoCollection.prototype._help = function () {  
  internal.print(helpArangoCollection);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  var result;

  if (this._type === null) {
    this.refresh();
  }

  return this._type;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the properties of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.properties = function (properties) {
  var requestResult;

  if (properties === undefined) {
    requestResult = this._database._connection.GET(this._baseurl("properties"));

    arangosh.checkRequestResult(requestResult);
  }
  else {
    var body = {};

    if (properties.hasOwnProperty("waitForSync")) {
      body.waitForSync = properties.waitForSync;
    }

    requestResult = this._database._connection.PUT(this._baseurl("properties"),
      JSON.stringify(body));

    arangosh.checkRequestResult(requestResult);
  }

  var result = { 
    waitForSync : requestResult.waitForSync,
    journalSize : requestResult.journalSize,
    isVolatile : requestResult.isVolatile
  };
    
  if (requestResult.keyOptions !== undefined) {
    result.keyOptions = requestResult.keyOptions;
  }
    
  return result;
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

  arangosh.checkRequestResult(requestResult);

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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   index functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all indexes
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.getIndexes = function () {
  var requestResult = this._database._connection.GET(this._indexurl());

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
/// @brief deletes one index
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
/// @brief adds a bitarray index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureBitarray = function () {
  var i;
  var body;
  var fields = [];

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  body = { type : "bitarray", unique : false, fields : fields };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a cap constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureCapConstraint = function (size) {
  var body;

  body = { type : "cap", size : size };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a unique skip-list index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueSkiplist = function () {
  var i;
  var body;
  var fields = [];

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  body = { type : "skiplist", unique : true, fields : fields };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a skip-list index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureSkiplist = function () {
  var i;
  var body;
  var fields = [];

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  body = { type : "skiplist", unique : false, fields : fields };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a fulltext index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureFulltextIndex = function (attribute, minLength) {
  var minLengthValue = minLength || undefined;
  var body;

  body = {
    type: "fulltext",
    minLength: minLengthValue,
    fields: [ attribute ]
  };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a unique constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueConstraint = function () {
  var i;
  var body;
  var fields = [];

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  body = { type : "hash", unique : true, fields : fields };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a hash index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureHashIndex = function () {
  var i;
  var body;
  var fields = [];

  for (i = 0;  i < arguments.length;  ++i) {
    fields.push(arguments[i]);
  }

  body = { type : "hash", unique : false, fields : fields };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an geo index
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
      geoJson : lon, 
      constraint : false
    };
  }
  else if (lon === undefined) {
    body = {
      type : "geo", 
      fields : [ lat ],
      geoJson : false, 
      constraint : false 
    };
  }
  else {
    body = {
      type : "geo",
      fields : [ lat, lon ],
      constraint : false
    };
  }

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an geo constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoConstraint = function (lat, lon, ignoreNull) {
  var body;

  // only two parameter
  if (ignoreNull === undefined) {
    ignoreNull = lon;

    if (typeof ignoreNull !== "boolean") {
      throw "usage: ensureGeoConstraint(<lat>, <lon>, <ignore-null>)"
          + " or ensureGeoConstraint(<lat>, <geo-json>, <ignore-null>)";
    }

    body = {
      type : "geo",
      fields : [ lat ],
      geoJson : false, 
      constraint : true,
      ignoreNull : ignoreNull 
    };
  }

  // three parameter
  else {
    if (typeof lon === "boolean") {
      body = {
        type : "geo",
        fields : [ lat ],
        geoJson : lon,
        constraint : true,
        ignoreNull : ignoreNull
      };
    }
    else {
      body = {
        type : "geo",
        fields : [ lat, lon ],
        constraint : true,
        ignoreNull : ignoreNull
      };
    }
  }

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                document functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

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
/// @brief gets a random element from the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.any = function () {
  var requestResult = this._database._connection.PUT("/_api/simple/any",
    JSON.stringify({collection: this._name}));

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
    "/_api/simple/first-example", JSON.stringify(data));

  if (requestResult !== null
      && requestResult.error === true 
      && requestResult.errorNum === internal.errors.ERROR_HTTP_NOT_FOUND.code) {
    return null;
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.document;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a document in the collection
/// note: this method is used to save documents and edges, but save() has a
/// different signature for both. For document collections, the signature is
/// save(<data>, <waitForSync>), whereas for edge collections, the signature is
/// save(<from>, <to>, <data>, <waitForSync>)
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.save = function (from, to, data, waitForSync) {
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

  var requestResult = this._database._connection.POST(url, JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document in the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.remove = function (id, overwrite, waitForSync) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var policy = "";

  if (overwrite) {
    policy = "?policy=last";
  }

  var url = this._documenturl(id) + policy;
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
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replace = function (id, data, overwrite, waitForSync) { 
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var policy = "";

  if (overwrite) {
    policy = "?policy=last";
  }

  var url = this._documenturl(id) + policy;
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
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.update = function (id, data, overwrite, keepNull, waitForSync) { 
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  // set default value for keepNull
  var keepNullValue = ((typeof keepNull === "undefined") ? true : keepNull);
  var params = "?keepNull=" + (keepNullValue ? "true" : "false");

  if (overwrite) {
    params += "&policy=last";
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

  var requestResult = this._database._connection.PUT(
    "/_api/simple/remove-by-example",
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

  var requestResult = this._database._connection.PUT(
    "/_api/simple/replace-by-example",
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

  var requestResult = this._database._connection.PUT(
    "/_api/simple/update-by-example",
    JSON.stringify(data));
  
  arangosh.checkRequestResult(requestResult);

  return requestResult.updated;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:

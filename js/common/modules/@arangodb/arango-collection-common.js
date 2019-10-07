/*jshint strict: false, unused: false, maxlen: 200 */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoCollection
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var ArangoCollection = require('@arangodb/arango-collection').ArangoCollection;

var arangodb = require('@arangodb');

var ArangoError = arangodb.ArangoError;
var sprintf = arangodb.sprintf;
var db = arangodb.db;

var simple = require('@arangodb/simple-query');

var SimpleQueryAll = simple.SimpleQueryAll;
var SimpleQueryByExample = simple.SimpleQueryByExample;
var SimpleQueryByCondition = simple.SimpleQueryByCondition;
var SimpleQueryRange = simple.SimpleQueryRange;
var SimpleQueryGeo = simple.SimpleQueryGeo;
var SimpleQueryNear = simple.SimpleQueryNear;
var SimpleQueryWithin = simple.SimpleQueryWithin;
var SimpleQueryWithinRectangle = simple.SimpleQueryWithinRectangle;
var SimpleQueryFulltext = simple.SimpleQueryFulltext;

// //////////////////////////////////////////////////////////////////////////////
// / @brief collection is corrupted
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_CORRUPTED = 0;

// //////////////////////////////////////////////////////////////////////////////
// / @brief collection is new born
// / @deprecated
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_NEW_BORN = 1;

// //////////////////////////////////////////////////////////////////////////////
// / @brief collection is unloaded
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_UNLOADED = 2;

// //////////////////////////////////////////////////////////////////////////////
// / @brief collection is loaded
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_LOADED = 3;

// //////////////////////////////////////////////////////////////////////////////
// / @brief collection is unloading
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_UNLOADING = 4;

// //////////////////////////////////////////////////////////////////////////////
// / @brief collection is deleted
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_DELETED = 5;

// //////////////////////////////////////////////////////////////////////////////
// / @brief collection is currently loading
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_LOADING = 6;

// //////////////////////////////////////////////////////////////////////////////
// / @brief document collection
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.TYPE_DOCUMENT = 2;

// //////////////////////////////////////////////////////////////////////////////
// / @brief edge collection
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.TYPE_EDGE = 3;

ArangoCollection.prototype.isArangoCollection = true;

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints a collection
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._PRINT = function (context) {
  var status = 'unknown';
  var type = 'unknown';
  var name = this.name();

  switch (this.status()) {
    case ArangoCollection.STATUS_NEW_BORN:
      status = 'new born';
      break;
    case ArangoCollection.STATUS_UNLOADED:
      status = 'unloaded';
      break;
    case ArangoCollection.STATUS_UNLOADING:
      status = 'unloading';
      break;
    case ArangoCollection.STATUS_LOADED:
      status = 'loaded';
      break;
    case ArangoCollection.STATUS_CORRUPTED:
      status = 'corrupted';
      break;
    case ArangoCollection.STATUS_DELETED:
      status = 'deleted';
      break;
  }

  switch (this.type()) {
    case ArangoCollection.TYPE_DOCUMENT:
      type = 'document';
      break;
    case ArangoCollection.TYPE_EDGE:
      type = 'edge';
      break;
  }

  var colors = require('internal').COLORS;
  var useColor = context.useColor;

  context.output += '[ArangoCollection ';
  if (useColor) { context.output += colors.COLOR_NUMBER; }
  context.output += this._id;
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += ', "';
  if (useColor) { context.output += colors.COLOR_STRING; }
  context.output += name || 'unknown';
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += '" (type ' + type + ', status ' + status + ')]';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief converts into a string
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toString = function () {
  return '[ArangoCollection: ' + this._id + ']';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief qualifies a given document key
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.documentId = function (documentKey) {
  if (documentKey && typeof documentKey !== "string") {
    documentKey = String(documentKey);
  }
  if (!arangodb.isValidDocumentKey(documentKey)) {
    throw new ArangoError({
      errorNum: arangodb.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code,
      errorMessage: arangodb.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.message
    });
  }
  return `${this.name()}/${documentKey}`;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionAll
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.all = function () {
  return new SimpleQueryAll(this);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionByExample
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byExample = function (example) {
  var e;
  var i;

  // example is given as only argument
  if (arguments.length === 1) {
    e = example;
  }

  // example is given as list
  else {
    e = {};

    // create a REAL array, otherwise JSON.stringify will fail
    for (i = 0;  i < arguments.length;  i += 2) {
      e[arguments[i]] = arguments[i + 1];
    }
  }

  return new SimpleQueryByExample(this, e);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionRange
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.range = function (name, left, right) {
  return new SimpleQueryRange(this, name, left, right, 0);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionClosedRange
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.closedRange = function (name, left, right) {
  return new SimpleQueryRange(this, name, left, right, 1);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionGeo
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.geo = function (loc, order) {
  var idx;

  var locateGeoIndex1 = function (collection, loc, order) {
    var inds = collection.getIndexes();
    var i;

    for (i = 0;  i < inds.length;  ++i) {
      var index = inds[i];

      if (index.type === 'geo1' || (index.type === 'geo' && index.fields.length === 1)) {
        if (index.fields[0] === loc && index.geoJson === order) {
          return index;
        }
      }
    }

    return null;
  };

  var locateGeoIndex2 = function (collection, lat, lon) {
    var inds = collection.getIndexes();
    var i;

    for (i = 0;  i < inds.length;  ++i) {
      var index = inds[i];

      if (index.type === 'geo2' || (index.type === 'geo' && index.fields.length === 2)) {
        if (index.fields[0] === lat && index.fields[1] === lon) {
          return index;
        }
      }
    }

    return null;
  };

  if (order === undefined) {
    if (typeof loc === 'object') {
      idx = this.index(loc);
    }else {
      idx = locateGeoIndex1(this, loc, false);
    }
  }
  else if (typeof order === 'boolean') {
    idx = locateGeoIndex1(this, loc, order);
  }else {
    idx = locateGeoIndex2(this, loc, order);
  }

  if (idx === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.code;
    err.errorMessage = require('internal').sprintf(arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message, this.name());
    throw err;
  }

  return new SimpleQueryGeo(this, idx.id);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionNear
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.near = function (lat, lon) {
  return new SimpleQueryNear(this, lat, lon);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionWithin
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.within = function (lat, lon, radius) {
  return new SimpleQueryWithin(this, lat, lon, radius);
};

ArangoCollection.prototype.withinRectangle = function (lat1, lon1, lat2, lon2) {
  return new SimpleQueryWithinRectangle(this, lat1, lon1, lat2, lon2);
};

ArangoCollection.prototype.fulltext = function (attribute, query, iid) {
  return new SimpleQueryFulltext(this, attribute, query, iid);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock collectionIterate
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.iterate = function (iterator, options) {
  var probability = 1.0;
  var limit = null;
  var stmt;
  var cursor;
  var pos;

  // TODO: this is not optimal for the client, there should be an HTTP call handling
  // everything on the server

  if (options !== undefined) {
    if (options.hasOwnProperty('probability')) {
      probability = options.probability;
    }

    if (options.hasOwnProperty('limit')) {
      limit = options.limit;
    }
  }

  if (limit === null) {
    if (probability >= 1.0) {
      cursor = this.all();
    }else {
      stmt = sprintf('FOR d IN %s FILTER rand() >= @prob RETURN d', this.name());
      stmt = db._createStatement({ query: stmt });

      if (probability < 1.0) {
        stmt.bind('prob', probability);
      }

      cursor = stmt.execute();
    }
  }else {
    if (typeof limit !== 'number') {
      var error = new ArangoError();
      error.errorNum = arangodb.errors.ERROR_ILLEGAL_NUMBER.code;
      error.errorMessage = 'expecting a number, got ' + String(limit);

      throw error;
    }

    if (probability >= 1.0) {
      cursor = this.all().limit(limit);
    }else {
      stmt = sprintf('FOR d IN %s FILTER rand() >= @prob LIMIT %d RETURN d',
        this.name(), limit);
      stmt = db._createStatement({ query: stmt });

      if (probability < 1.0) {
        stmt.bind('prob', probability);
      }

      cursor = stmt.execute();
    }
  }

  pos = 0;

  while (cursor.hasNext()) {
    var document = cursor.next();

    iterator(document, pos);

    pos++;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock documentsCollectionRemoveByExample
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByExample = function (example, waitForSync, limit) {
  throw 'cannot call abstract removeByExample function';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock documentsCollectionReplaceByExample
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replaceByExample = function (example, newValue, waitForSync, limit) {
  throw 'cannot call abstract replaceByExample function';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock documentsCollectionUpdateByExample
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.updateByExample = function (example, newValue, keepNull, waitForSync, limit) {
  throw 'cannot call abstract updateExample function';
};


// //////////////////////////////////////////////////////////////////////////////
// / SECTION Indexes
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief add options from arguments to index specification
// //////////////////////////////////////////////////////////////////////////////

function addIndexOptions (body, parameters) {
  body.fields = [];

  var setOption = function (k) {
    if (! body.hasOwnProperty(k)) {
      body[k] = parameters[i][k];
    }
  };

  var i;
  for (i = 0; i < parameters.length; ++i) {
    if (typeof parameters[i] === 'string') {
      // set fields
      body.fields.push(parameters[i]);
    }
    else if (typeof parameters[i] === 'object' &&
      ! Array.isArray(parameters[i]) &&
      parameters[i] !== null) {
      // set arbitrary options
      Object.keys(parameters[i]).forEach(setOption);
      break;
    }
  }

  return body;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief ensures a hash index
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureHashIndex = function () {
  'use strict';

  return this.ensureIndex(addIndexOptions({
    type: 'hash',
  }, arguments));
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief ensures a unique constraint
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueConstraint = function () {
  'use strict';

  return this.ensureIndex(addIndexOptions({
    type: 'hash',
    unique: true
  }, arguments));
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief ensures a unique skip-list index
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueSkiplist = function () {
  'use strict';

  return this.ensureIndex(addIndexOptions({
    type: 'skiplist',
    unique: true
  }, arguments));
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief ensures a skip-list index
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureSkiplist = function () {
  'use strict';

  return this.ensureIndex(addIndexOptions({
    type: 'skiplist',
    unique: false
  }, arguments));
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief ensures a fulltext index
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureFulltextIndex = function (field, minLength) {
  'use strict';

  if (! Array.isArray(field)) {
    field = [ field ];
  }

  return this.ensureIndex({
    type: 'fulltext',
    minLength: minLength || undefined,
    fields: field
  });
};


// //////////////////////////////////////////////////////////////////////////////
// / @brief ensures a geo index
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoIndex = function (lat, lon) {
  'use strict';

  if (typeof lat !== 'string') {
    throw 'usage: ensureGeoIndex(<lat>, <lon>) or ensureGeoIndex(<loc>[, <geoJson>[, <pointsOnly>]])';
  }

  if (typeof lon === 'boolean') {
    return this.ensureIndex({
      type: 'geo1',
      fields: [ lat ],
      geoJson: lon
    });
  }

  if (lon === undefined) {
    return this.ensureIndex({
      type: 'geo1',
      fields: [ lat ],
      geoJson: false
    });
  }

  return this.ensureIndex({
    type: 'geo2',
    fields: [ lat, lon ]
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief ensures a geo constraint
// / since ArangoDB 2.5, this is just a redirection to ensureGeoIndex
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoConstraint = function (lat, lon) {
  return this.ensureGeoIndex(lat, lon);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief ensures a vertex-centric index
// //////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureVertexCentricIndex = function (...fields) {
  let options = fields.pop();
  if (!typeof options === 'object' || Array.isArray(options)) {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = 'usage: ensureVertexCentricIndex(<...fields>, options), options has to be an object';
    throw err;
  }
  let dir = options.direction;
  if (typeof dir === 'string') {
    dir = dir.toLowerCase();
  }
  if (fields.length === 0 && options.hasOwnProperty("fields") && Array.isArray(options.fields)) {
    // This copies the content of the array.
    fields = options.fields.slice(0);
  }
  if (fields.length === 0) {
    let err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = 'usage: ensureVertexCentricIndex(<...fields>, options), we need a non empty list of fields';
    throw err;
  }
  switch (dir) {
    case "outbound":
      fields.unshift("_from");
      break;
    case "inbound":
      fields.unshift("_to");
      break;
    default:
      let err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
      err.errorMessage = 'usage: ensureVertexCentricIndex(<...fields>, options), options.direction has to be "inbound" or "outbound"';
      throw err;
  }
  options.fields = fields;
  if (!options.hasOwnProperty('type')) {
    options.type = 'hash';
  }
  return this.ensureIndex(options);
};

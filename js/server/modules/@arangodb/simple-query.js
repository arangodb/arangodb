/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Arango Simple Query Language
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

var internal = require('internal');

var ArangoError = require('@arangodb').ArangoError;

var sq = require('@arangodb/simple-query-common');

var GeneralArrayCursor = sq.GeneralArrayCursor;
var SimpleQueryAll = sq.SimpleQueryAll;
var SimpleQueryArray = sq.SimpleQueryArray;
var SimpleQueryByExample = sq.SimpleQueryByExample;
var SimpleQueryFulltext = sq.SimpleQueryFulltext;
var SimpleQueryGeo = sq.SimpleQueryGeo;
var SimpleQueryNear = sq.SimpleQueryNear;
var SimpleQueryRange = sq.SimpleQueryRange;
var SimpleQueryWithin = sq.SimpleQueryWithin;
var SimpleQueryWithinRectangle = sq.SimpleQueryWithinRectangle;

// //////////////////////////////////////////////////////////////////////////////
// / @brief rewrites an index id by stripping the collection name from it
// //////////////////////////////////////////////////////////////////////////////

var rewriteIndex = function (id) {
  if (id === null || id === undefined) {
    return;
  }

  if (typeof id === 'string') {
    return id.replace(/^[a-zA-Z0-9_\-]+\//, '');
  }
  if (typeof id === 'object' && id.hasOwnProperty('id')) {
    return id.id.replace(/^[a-zA-Z0-9_\-]+\//, '');
  }
  return id;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief build a limit string
// //////////////////////////////////////////////////////////////////////////////

var limitString = function (skip, limit) {
  if (skip > 0 || limit > 0) {
    if (limit <= 0) {
      limit = 99999999999;
    }
    return 'LIMIT ' + parseInt(skip, 10) + ', ' + parseInt(limit, 10) + ' ';
  }
  return '';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.execute = function () {
  if (this._execution !== null) {
    return;
  }

  if (this._skip === null) {
    this._skip = 0;
  }

  var bindVars = { '@collection': this._collection.name() };

  var query = 'FOR doc IN @@collection ' + limitString(this._skip, this._limit) + ' RETURN doc';
  var documents = require('internal').db._query({ query, bindVars}).toArray();

  this._execution = new GeneralArrayCursor(documents);
  this._countQuery = documents.length;
  this._countTotal = documents.length;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a query-by-example
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.execute = function () {
  if (this._execution !== null) {
    return;
  }

  if (this._skip === null || this._skip <= 0) {
    this._skip = 0;
  }

  if (this._example === null ||
    typeof this._example !== 'object' ||
    Array.isArray(this._example)) {
    // invalid datatype for example
    var err = new ArangoError();
    err.errorNum = internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code;
    err.errorMessage = "invalid document type '" + (typeof this._example) + "'";
    throw err;
  }

  var bindVars = { '@collection': this._collection.name() };
  var filters = [];
  var self = this;
  Object.keys(this._example).forEach(function (key) {
    var value = self._example[key];
    filters.push('FILTER doc.`' + key.replace(/\`/g, '').split('.').join('`.`') + '` == ' + JSON.stringify(value));
  });

  var query = 'FOR doc IN @@collection ' + filters.join(' ') + ' ' +
    limitString(this._skip, this._limit) + ' RETURN doc';

  var documents = require('internal').db._query({ query, bindVars}).toArray();

  this._execution = new GeneralArrayCursor(documents);
  this._countQuery = documents.length;
  this._countTotal = documents.length;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a range query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype.execute = function () {
  if (this._execution !== null) {
    return;
  }

  var query = 'FOR doc IN @@collection ';
  var bindVars = {
    '@collection': this._collection.name(),
    attribute: this._attribute,
    left: this._left,
    right: this._right
  };

  if (this._type === 0) {
    query += 'FILTER doc.@attribute >= @left && doc.@attribute < @right ';
  } else if (this._type === 1) {
    query += 'FILTER doc.@attribute >= @left && doc.@attribute <= @right ';
  } else {
    throw 'unknown type';
  }

  query += limitString(this._skip, this._limit) + ' RETURN doc';

  var documents = require('internal').db._query({ query, bindVars}).toArray();

  this._execution = new GeneralArrayCursor(documents);
  this._countQuery = documents.length - this._skip;
  this._countTotal = documents.length;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a near query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.execute = function () {
  if (this._execution !== null) {
    return;
  }

  if (this._skip === null) {
    this._skip = 0;
  }

  if (this._skip < 0) {
    var err = new ArangoError();
    err.errorNum = internal.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = 'skip must be non-negative';
    throw err;
  }

  if (this._limit <= 0) {
    this._limit = 100;
  }
  
  let cluster = require('@arangodb/cluster');
  if (cluster.isCoordinator()) {
    if (this._distance === null || this._distance === undefined) {
      this._distance = '$distance';
    }
  }

  var bindVars = {
    '@collection': this._collection.name(),
    latitude: this._latitude,
    longitude: this._longitude,
    limit: parseInt((this._skip || 0) + (this._limit || 99999999999), 10)
  };

  var query;
  if (typeof this._distance === 'string') {
    query = 'FOR doc IN NEAR(@@collection, @latitude, @longitude, @limit, @distance) ';
    bindVars.distance = this._distance;
  } else {
    query = 'FOR doc IN NEAR(@@collection, @latitude, @longitude, @limit) ';
  }
  
  if (cluster.isCoordinator()) {
    query += 'SORT doc.@distance ';
  }

  query += limitString(this._skip, this._limit) + ' RETURN doc';

  let documents = require('internal').db._query({ query, bindVars }).toArray();

  this._execution = new GeneralArrayCursor(documents);
  this._countQuery = documents.length - this._skip;
  this._countTotal = documents.length;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a within query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.execute = function () {
  if (this._execution !== null) {
    return;
  }

  if (this._skip === null) {
    this._skip = 0;
  }

  if (this._skip < 0) {
    var err = new ArangoError();
    err.errorNum = internal.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = 'skip must be non-negative';
    throw err;
  }
  
  let cluster = require('@arangodb/cluster');
  if (cluster.isCoordinator()) {
    if (this._distance === null || this._distance === undefined) {
      this._distance = '$distance';
    }
  }

  var bindVars = {
    '@collection': this._collection.name(),
    latitude: this._latitude,
    longitude: this._longitude,
    radius: this._radius
  };

  var query;
  if (typeof this._distance === 'string') {
    query = 'FOR doc IN WITHIN(@@collection, @latitude, @longitude, @radius, @distance) ';
    bindVars.distance = this._distance;
  } else {
    query = 'FOR doc IN WITHIN(@@collection, @latitude, @longitude, @radius) ';
  }
  
  if (cluster.isCoordinator()) {
    query += 'SORT doc.@distance ';
  }

  query += limitString(this._skip, this._limit) + ' RETURN doc';

  let documents = require('internal').db._query({ query, bindVars }).toArray();

  this._execution = new GeneralArrayCursor(documents);
  this._countQuery = documents.length - this._skip;
  this._countTotal = documents.length;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a fulltext query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype.execute = function () {
  if (this._execution !== null) {
    return;
  }

  let bindVars = {
    '@collection': this._collection.name(),
    attribute: this._attribute,
    query: this._query
  };
  let query = 'FOR doc IN FULLTEXT(@@collection, @attribute, @query) ' +
              limitString(this._skip, this._limit) + ' RETURN doc';
  let documents = require('internal').db._query({ query, bindVars}).toArray();

  this._execution = new GeneralArrayCursor(documents);
  this._countQuery = documents.length - this._skip;
  this._countTotal = documents.length;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a within-rectangle query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle.prototype.execute = function () {
  var result;
  var documents;

  if (this._execution !== null) {
    return;
  }

  if (this._skip === null) {
    this._skip = 0;
  }

  if (this._skip < 0) {
    var err = new ArangoError();
    err.errorNum = internal.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = 'skip must be non-negative';
    throw err;
  }

  var distanceMeters = function (lat1, lon1, lat2, lon2) {
    var deltaLat = (lat2 - lat1) * Math.PI / 180;
    var deltaLon = (lon2 - lon1) * Math.PI / 180;
    var a = Math.sin(deltaLat / 2) * Math.sin(deltaLat / 2) +
      Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
      Math.sin(deltaLon / 2) * Math.sin(deltaLon / 2);
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));

    return 6378.137 /* radius of earth in kilometers */
      * c
      * 1000; // kilometers to meters
  };

  var diameter = distanceMeters(this._latitude1, this._longitude1, this._latitude2, this._longitude2);
  var midpoint = [
    this._latitude1 + (this._latitude2 - this._latitude1) * 0.5,
    this._longitude1 + (this._longitude2 - this._longitude1) * 0.5
  ];

  result = this._collection.within(midpoint[0], midpoint[1], diameter).toArray();

  var idx = this._collection.index(this._index);
  var latLower, latUpper, lonLower, lonUpper;

  if (this._latitude1 < this._latitude2) {
    latLower = this._latitude1;
    latUpper = this._latitude2;
  } else {
    latLower = this._latitude2;
    latUpper = this._latitude1;
  }

  if (this._longitude1 < this._longitude2) {
    lonLower = this._longitude1;
    lonUpper = this._longitude2;
  } else {
    lonLower = this._longitude2;
    lonUpper = this._longitude1;
  }

  var deref = function (doc, parts) {
    if (parts.length === 1) {
      return doc[parts[0]];
    }

    var i = 0;
    try {
      while (i < parts.length && doc !== null && doc !== undefined) {
        doc = doc[parts[i]];
        ++i;
      }
      return doc;
    } catch (err) {
      return null;
    }
  };

  documents = [];
  if (idx.type === 'geo1' || (idx.type === 'geo' && idx.fields.length === 1)) {
    // geo1, we have both coordinates in a list
    var attribute = idx.fields[0];
    var parts = attribute.split('.');

    if (idx.geoJson) {
      result.forEach(function (document) {
        var doc = deref(document, parts);
        if (!Array.isArray(doc)) {
          return;
        }

        // check if within bounding rectangle
        // first list value is longitude, then latitude
        if (doc[1] >= latLower && doc[1] <= latUpper &&
          doc[0] >= lonLower && doc[0] <= lonUpper) {
          documents.push(document);
        }
      });
    } else {
      result.forEach(function (document) {
        var doc = deref(document, parts);
        if (!Array.isArray(doc)) {
          return;
        }

        // check if within bounding rectangle
        // first list value is latitude, then longitude
        if (doc[0] >= latLower && doc[0] <= latUpper &&
          doc[1] >= lonLower && doc[1] <= lonUpper) {
          documents.push(document);
        }
      });
    }
  } else {
    // geo2, we have dedicated latitude and longitude attributes
    var latAtt = idx.fields[0], lonAtt = idx.fields[1];
    var latParts = latAtt.split('.');
    var lonParts = lonAtt.split('.');

    result.forEach(function (document) {
      var latDoc = deref(document, latParts);
      if (latDoc === null || latDoc === undefined) {
        return;
      }
      var lonDoc = deref(document, lonParts);
      if (lonDoc === null || lonDoc === undefined) {
        return;
      }

      // check if within bounding rectangle
      if (latDoc >= latLower && latDoc <= latUpper &&
        lonDoc >= lonLower && lonDoc <= lonUpper) {
        documents.push(document);
      }
    });
  }

  documents = {
    documents: documents,
    count: result.length,
    total: result.length
  };

  if (this._limit > 0) {
    documents.documents = documents.documents.slice(0, this._skip + this._limit);
    documents.count = documents.documents.length;
  }

  this._execution = new GeneralArrayCursor(documents.documents, this._skip, null);
  this._countQuery = documents.total - this._skip;
  this._countTotal = documents.total;
};

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryArray = SimpleQueryArray;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryFulltext = SimpleQueryFulltext;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryRange = SimpleQueryRange;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.SimpleQueryWithinRectangle = SimpleQueryWithinRectangle;

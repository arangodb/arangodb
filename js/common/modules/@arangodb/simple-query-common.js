/*jshint strict: false */

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

const arangodb = require('@arangodb');
const GeneralArrayCursor = require("@arangodb/arango-cursor").GeneralArrayCursor;

var ArangoError = arangodb.ArangoError;

// forward declaration
var SimpleQueryArray;
var SimpleQueryNear;
var SimpleQueryWithin;
var SimpleQueryWithinRectangle;

// //////////////////////////////////////////////////////////////////////////////
// / @brief simple query
// //////////////////////////////////////////////////////////////////////////////

function SimpleQuery () {
  this._execution = null;
  this._skip = 0;
  this._limit = null;
  this._countQuery = null;
  this._countTotal = null;
  this._batchSize = null;
}

SimpleQuery.prototype.isArangoResultSet = true;

// //////////////////////////////////////////////////////////////////////////////
// / @brief join limits
// //////////////////////////////////////////////////////////////////////////////

function joinLimits (query, limit) {
  // original limit is 0, keep it
  if (query._limit === 0) {
    query = query.clone();
  }

  // new limit is 0, use it
  else if (limit === 0) {
    query = query.clone();
    query._limit = 0;
  }

  // no old limit, use new limit
  else if (query._limit === null) {
    query = query.clone();
    query._limit = limit;
  }

  // use the smaller one
  else {
    query = query.clone();

    if (limit < query._limit) {
      query._limit = limit;
    }
  }

  return query;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a query
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.clone = function () {
  throw 'cannot clone abstract query';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock queryExecute
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.execute = function () {
  throw 'cannot execute abstract query';
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock queryLimit
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.limit = function (limit) {
  if (this._execution !== null) {
    throw 'query is already executing';
  }

  if (limit < 0) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_BAD_PARAMETER;
    err.errorMessage = 'limit must be non-negative';
    throw err;
  }

  return joinLimits(this, limit);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock querySkip
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.skip = function (skip) {
  var query;
  var documents;

  if (skip === undefined || skip === null) {
    skip = 0;
  }

  if (this._execution !== null) {
    throw 'query is already executing';
  }

  // no limit set, use or add skip
  if (this._limit === null) {
    query = this.clone();

    if (this._skip === null || this._skip === 0) {
      query._skip = skip;
    }else {
      query._skip += skip;
    }
  }

  // limit already set
  else {
    documents = this.clone().toArray();

    query = new SimpleQueryArray(documents);
    query._skip = skip;
    query._countTotal = documents._countTotal;
  }

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief converts into an array
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.toArray = function () {
  var result;

  this.execute();

  result = [];

  while (this.hasNext()) {
    result.push(this.next());
  }

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock cursorGetBatchSize
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.getBatchSize = function () {
  return this._batchSize;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock cursorSetBatchSize
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.setBatchSize = function (value) {
  if (value >= 1) {
    this._batchSize = value;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock cursorCount
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.count = function (applyPagination) {
  this.execute();

  if (applyPagination === undefined || ! applyPagination) {
    return this._countTotal;
  }

  return this._countQuery;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock cursorHasNext
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.hasNext = function () {
  this.execute();

  return this._execution.hasNext();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock cursorNext
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.next = function () {
  this.execute();

  return this._execution.next();
};

SimpleQuery.prototype[Symbol.iterator] = function * () {
  this.execute();
  for (const item of this._execution) {
    yield item;
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock cursorDispose
// //////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.dispose = function () {
  if (this._execution !== null) {
    this._execution.dispose();
  }

  this._execution = null;
  this._countQuery = null;
  this._countTotal = null;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief all query
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryAll (collection) {
  this._collection = collection;
}

SimpleQueryAll.prototype = new SimpleQuery();
SimpleQueryAll.prototype.constructor = SimpleQueryAll;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.clone = function () {
  var query;

  query = new SimpleQueryAll(this._collection);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryAll(' + this._collection.name() + ')';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief array query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryArray = function (documents) {
  this._documents = documents;
};

SimpleQueryArray.prototype = new SimpleQuery();
SimpleQueryArray.prototype.constructor = SimpleQueryArray;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.clone = function () {
  var query;

  query = new SimpleQueryArray(this._documents);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.execute = function () {
  if (this._execution === null) {
    if (this._skip === null) {
      this._skip = 0;
    }

    this._execution = new GeneralArrayCursor(this._documents, this._skip, this._limit);
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryArray(documents)';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief query-by-example
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryByExample (collection, example) {
  this._collection = collection;
  this._example = example;
}

SimpleQueryByExample.prototype = new SimpleQuery();
SimpleQueryByExample.prototype.constructor = SimpleQueryByExample;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a query-by-example
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.clone = function () {
  var query;

  query = new SimpleQueryByExample(this._collection, this._example);
  query._skip = this._skip;
  query._limit = this._limit;
  query._type = this._type;
  query._index = this._index;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print a query-by-example
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryByExample(' + this._collection.name() + ')';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief query-by-condition
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryByCondition (collection, condition) {
  this._collection = collection;
  this._condition = condition;
}

SimpleQueryByCondition.prototype = new SimpleQuery();
SimpleQueryByCondition.prototype.constructor = SimpleQueryByCondition;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a query-by-condition
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype.clone = function () {
  var query;

  query = new SimpleQueryByCondition(this._collection, this._condition);
  query._skip = this._skip;
  query._limit = this._limit;
  query._type = this._type;
  query._index = this._index;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print a query-by-condition
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryByCondition(' + this._collection.name() + ')';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief range query
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryRange (collection, attribute, left, right, type) {
  this._collection = collection;
  this._attribute = attribute;
  this._left = left;
  this._right = right;
  this._type = type;
}

SimpleQueryRange.prototype = new SimpleQuery();
SimpleQueryRange.prototype.constructor = SimpleQueryRange;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a range query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype.clone = function () {
  var query;

  query = new SimpleQueryRange(this._collection,
    this._attribute,
    this._left,
    this._right,
    this._type);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints a range query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryRange(' + this._collection.name() + ')';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief geo index
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryGeo (collection, index) {
  this._collection = collection;
  this._index = index;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints a geo index
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype._PRINT = function (context) {
  var text;

  text = 'GeoIndex('
    + this._collection.name()
    + ', '
    + this._index
    + ')';

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructs a near query for an index
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.near = function (lat, lon) {
  return new SimpleQueryNear(this._collection, lat, lon, this._index);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructs a within query for an index
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.within = function (lat, lon, radius) {
  return new SimpleQueryWithin(this._collection, lat, lon, radius, this._index);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructs a within-rectangle query for an index
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.withinRectangle = function (lat1, lon1, lat2, lon2) {
  return new SimpleQueryWithinRectangle(this._collection, lat1, lon1, lat2, lon2, this._index);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief near query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryNear = function (collection, latitude, longitude, iid) {
  var idx;
  var i;

  this._collection = collection;
  this._latitude = latitude;
  this._longitude = longitude;
  this._index = (iid === undefined ? null : iid);
  this._distance = null;

  if (iid === undefined) {
    idx = collection.getIndexes();

    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];

      if (index.type === 'geo' || index.type === 'geo1' || index.type === 'geo2') {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.id < this._index) {
          this._index = index.id;
        }
      }
    }
  }

  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_GEO_INDEX_MISSING;
    err.errorMessage = require('internal').sprintf(
      arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message,
      collection.name());
    throw err;
  }
};

SimpleQueryNear.prototype = new SimpleQuery();
SimpleQueryNear.prototype.constructor = SimpleQueryNear;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a near query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.clone = function () {
  var query;

  query = new SimpleQueryNear(this._collection, this._latitude, this._longitude, this._index);
  query._skip = this._skip;
  query._limit = this._limit;
  query._distance = this._distance;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints a near query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryNear('
    + this._collection.name()
    + ', '
    + this._latitude
    + ', '
    + this._longitude
    + ', '
    + this._index
    + ')';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief adds the distance attribute
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.distance = function (attribute) {
  var clone;

  clone = this.clone();

  if (attribute) {
    clone._distance = attribute;
  }else {
    clone._distance = 'distance';
  }

  return clone;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief within query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin = function (collection, latitude, longitude, radius, iid) {
  var idx;
  var i;

  this._collection = collection;
  this._latitude = latitude;
  this._longitude = longitude;
  this._index = (iid === undefined ? null : iid);
  this._radius = radius;
  this._distance = null;

  if (iid === undefined) {
    idx = collection.getIndexes();

    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];

      if (index.type === 'geo' || index.type === 'geo1' || index.type === 'geo2') {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.id < this._index) {
          this._index = index.id;
        }
      }
    }
  }

  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_GEO_INDEX_MISSING;
    err.errorMessage = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message;
    throw err;
  }
};

SimpleQueryWithin.prototype = new SimpleQuery();
SimpleQueryWithin.prototype.constructor = SimpleQueryWithin;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a within query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.clone = function () {
  var query;

  query = new SimpleQueryWithin(this._collection,
    this._latitude,
    this._longitude,
    this._radius,
    this._index);
  query._skip = this._skip;
  query._limit = this._limit;
  query._distance = this._distance;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints a within query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryWithin('
    + this._collection.name()
    + ', '
    + this._latitude
    + ', '
    + this._longitude
    + ', '
    + this._radius
    + ', '
    + this._index
    + ')';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief adds the distance attribute
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.distance = function (attribute) {
  var clone;

  clone = this.clone();

  if (attribute) {
    clone._distance = attribute;
  }else {
    clone._distance = 'distance';
  }

  return clone;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief within-rectangle query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle = function (collection, latitude1, longitude1, latitude2, longitude2, iid) {
  var idx;
  var i;

  this._collection = collection;
  this._latitude1 = latitude1;
  this._longitude1 = longitude1;
  this._latitude2 = latitude2;
  this._longitude2 = longitude2;
  this._index = (iid === undefined ? null : iid);

  if (iid === undefined) {
    idx = collection.getIndexes();

    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];

      if (index.type === 'geo' || index.type === 'geo1' || index.type === 'geo2') {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.id < this._index) {
          this._index = index.id;
        }
      }
    }
  }

  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_GEO_INDEX_MISSING;
    err.errorMessage = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message;
    throw err;
  }
};

SimpleQueryWithinRectangle.prototype = new SimpleQuery();
SimpleQueryWithinRectangle.prototype.constructor = SimpleQueryWithinRectangle;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a within-rectangle query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle.prototype.clone = function () {
  var query;

  query = new SimpleQueryWithinRectangle(this._collection,
    this._latitude1,
    this._longitude1,
    this._latitude2,
    this._longitude2,
    this._index);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints a within-rectangle query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryWithinRectangle('
    + this._collection.name()
    + ', '
    + this._latitude1
    + ', '
    + this._longitude1
    + ', '
    + this._latitude2
    + ', '
    + this._longitude2
    + ', '
    + this._index
    + ')';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief fulltext query
// //////////////////////////////////////////////////////////////////////////////

function SimpleQueryFulltext (collection, attribute, query, iid) {
  this._collection = collection;
  this._attribute = attribute;
  this._query = query;
  this._index = (iid === undefined ? null : iid);

  if (iid === undefined) {
    var idx = collection.getIndexes();
    var i;

    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];

      if (index.type === 'fulltext' && index.fields[0] === attribute) {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.indexSubstrings && ! this._index.indexSubstrings) {
          // prefer indexes that have substrings indexed
          this._index = index.id;
        }
      }
    }
  }

  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_FULLTEXT_INDEX_MISSING;
    err.errorMessage = require('internal').sprintf(
      arangodb.errors.ERROR_QUERY_FULLTEXT_INDEX_MISSING.message,
      collection.name());
    throw err;
  }
}

SimpleQueryFulltext.prototype = new SimpleQuery();
SimpleQueryFulltext.prototype.constructor = SimpleQueryFulltext;

// //////////////////////////////////////////////////////////////////////////////
// / @brief clones a fulltext query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype.clone = function () {
  var query;

  query = new SimpleQueryFulltext(this._collection, this._attribute, this._query, this._index);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints a fulltext query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype._PRINT = function (context) {
  var text;

  text = 'SimpleQueryFulltext('
    + this._collection.name()
    + ', '
    + this._attribute
    + ', "'
    + this._query
    + '")';

  if (this._skip !== null && this._skip !== 0) {
    text += '.skip(' + this._skip + ')';
  }

  if (this._limit !== null) {
    text += '.limit(' + this._limit + ')';
  }

  context.output += text;
};

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryArray = SimpleQueryArray;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryByCondition = SimpleQueryByCondition;
exports.SimpleQueryRange = SimpleQueryRange;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.SimpleQueryWithinRectangle = SimpleQueryWithinRectangle;
exports.SimpleQueryFulltext = SimpleQueryFulltext;

/* jshint strict: false */
/* global SYS_IS_V8_BUILD */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangosh = require('@arangodb/arangosh');

var ArangoQueryCursor = require('@arangodb/arango-query-cursor').ArangoQueryCursor;

var sq = require('@arangodb/simple-query-common');

var GeneralArrayCursor = sq.GeneralArrayCursor;
var SimpleQueryAll = sq.SimpleQueryAll;
var SimpleQueryArray = sq.SimpleQueryArray;
var SimpleQueryByExample = sq.SimpleQueryByExample;
var SimpleQueryByCondition = sq.SimpleQueryByCondition;
var SimpleQueryFulltext = sq.SimpleQueryFulltext;
var SimpleQueryGeo = sq.SimpleQueryGeo;
var SimpleQueryNear = sq.SimpleQueryNear;
var SimpleQueryRange = sq.SimpleQueryRange;
var SimpleQueryWithin = sq.SimpleQueryWithin;
var SimpleQueryWithinRectangle = sq.SimpleQueryWithinRectangle;

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes an all query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var query = 'FOR doc IN @@collection';
    var bindVars = { '@collection': this._collection.name() };
    var cursorOptions = {};

    if (this._skip !== null && this._skip > 0) {
      query += ' LIMIT @skip';
      bindVars.skip = this._skip;
      if (this._limit !== null && this._limit > 0) {
        query += ', @limit';
        bindVars.limit = this._limit;
      } else {
        query += ', @limit';
        bindVars.limit = 99999999999;
      }
    } else if (this._limit !== null && this._limit > 0) {
      query += ' LIMIT @limit';
      bindVars.limit = this._limit;
    }

    query += ' RETURN doc';

    if (this._batchSize !== null) {
      cursorOptions.batchSize = this._batchSize;
    }
    cursorOptions.count = true;

    this._execution = this._collection._database._query(query, bindVars, cursorOptions);

    if (this._execution.hasOwnProperty('count')) {
      this._countQuery = this._execution.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a query-by-example
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var bindVars = { '@collection': this._collection.name() };
    var filters = [];
    var self = this;
    Object.keys(this._example).forEach(function (key) {
      var value = self._example[key];
      // Ensure key is a string and process it safely
      var keyStr = String(key);
      if (keyStr.length === 0) {
        // Skip empty keys
        return;
      }
      // Remove backticks, split on dots, and join with backtick-dot-backtick
      // Filter out empty parts to avoid issues with keys starting with dots
      var processedKey = keyStr.replace(/\`/g, '').split('.').filter(function(part) { return part.length > 0; }).join('`.`');
      if (processedKey.length === 0) {
        // Skip if processing results in empty string
        return;
      }
      // Always prefix with 'doc.`' to ensure proper variable reference
      filters.push('doc.`' + processedKey + '` == ' + JSON.stringify(value));
    });

    var query = 'FOR doc IN @@collection';
    if (filters.length > 0) {
      query += ' FILTER ' + filters.join(' AND ') + ' ';
    }

    if (this._skip !== null && this._skip > 0) {
      query += ' LIMIT @skip';
      bindVars.skip = this._skip;
      if (this._limit !== null && this._limit > 0) {
        query += ', @limit';
        bindVars.limit = this._limit;
      } else {
        query += ', @limit';
        bindVars.limit = 99999999999;
      }
    } else if (this._limit !== null && this._limit > 0) {
      query += ' LIMIT @limit';
      bindVars.limit = this._limit;
    }

    query += ' RETURN doc';

    var cursorOptions = {};
    if (this._batchSize !== null) {
      cursorOptions.batchSize = this._batchSize;
    }
    cursorOptions.count = true;

    this._execution = this._collection._database._query(query, bindVars, cursorOptions);

    if (this._execution.hasOwnProperty('count')) {
      this._countQuery = this._execution.count;
      this._countTotal = this._execution.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a query-by-condition
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    // Convert condition object to AQL FILTER conditions
    var bindVars = { '@collection': this._collection.name() };
    var filters = [];
    var varCount = 0;
    var self = this;
    
    // Condition is typically an object with attribute-value pairs or AQL expressions
    if (typeof this._condition === 'object' && !Array.isArray(this._condition)) {
      Object.keys(this._condition).forEach(function (key) {
        var value = self._condition[key];
        // Ensure key is a string and process it safely
        var keyStr = String(key);
        if (keyStr.length === 0) {
          // Skip empty keys
          return;
        }
        // Remove backticks, split on dots, and join with backtick-dot-backtick
        var processedKey = keyStr.replace(/\`/g, '').split('.').filter(function(part) { return part.length > 0; }).join('`.`');
        if (processedKey.length === 0) {
          // Skip if processing results in empty string
          return;
        }
        var varName = 'cond' + varCount++;
        filters.push('doc.`' + processedKey + '` == @' + varName);
        bindVars[varName] = value;
      });
    }

    var query = 'FOR doc IN @@collection';
    if (filters.length > 0) {
      query += ' FILTER ' + filters.join(' && ') + ' ';
    }

    if (this._skip !== null && this._skip > 0) {
      query += ' LIMIT @skip';
      bindVars.skip = this._skip;
      if (this._limit !== null && this._limit > 0) {
        query += ', @limit';
        bindVars.limit = this._limit;
      } else {
        query += ', @limit';
        bindVars.limit = 99999999999;
      }
    } else if (this._limit !== null && this._limit > 0) {
      query += ' LIMIT @limit';
      bindVars.limit = this._limit;
    }

    query += ' RETURN doc';

    var cursorOptions = {};
    if (this._batchSize !== null) {
      cursorOptions.batchSize = this._batchSize;
    }
    cursorOptions.count = true;

    this._execution = this._collection._database._query(query, bindVars, cursorOptions);

    if (this._execution.hasOwnProperty('count')) {
      this._countQuery = this._execution.count;
      this._countTotal = this._execution.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a range query
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

SimpleQueryRange.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
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
    
    var cursorOptions = {};
    if (this._batchSize !== null) {
      cursorOptions.batchSize = this._batchSize;
    }
    cursorOptions.count = true;
    
    this._execution = this._collection._database._query(query, bindVars, cursorOptions);

    if (this._execution.hasOwnProperty('count')) {
      this._countQuery = this._execution.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a near query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var query = 'FOR doc IN @@collection';
    var bindVars = { '@collection': this._collection.name() };
    
    if (this._index !== null) {
      query += ' USE INDEX @index';
      bindVars.index = this._index;
    }
    
    query += ' FILTER GEO_DISTANCE([@lat, @lon], doc.@attribute) <= @distance';
    bindVars.lat = this._latitude;
    bindVars.lon = this._longitude;
    bindVars.attribute = this._attribute || 'location';
    bindVars.distance = this._distance !== null ? this._distance : 1000000;
    
    query += ' SORT GEO_DISTANCE([@lat, @lon], doc.@attribute) ASC';
    
    if (this._skip !== null && this._skip > 0) {
      query += ' LIMIT @skip';
      bindVars.skip = this._skip;
      if (this._limit !== null && this._limit > 0) {
        query += ', @limit';
        bindVars.limit = this._limit;
      } else {
        query += ', @limit';
        bindVars.limit = 99999999999;
      }
    } else if (this._limit !== null && this._limit > 0) {
      query += ' LIMIT @limit';
      bindVars.limit = this._limit;
    }
    
    query += ' RETURN doc';

    var cursorOptions = {};
    if (this._batchSize !== null) {
      cursorOptions.batchSize = this._batchSize;
    }
    cursorOptions.count = true;

    this._execution = this._collection._database._query(query, bindVars, cursorOptions);

    if (this._execution.hasOwnProperty('count')) {
      this._countQuery = this._execution.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a within query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var query = 'FOR doc IN @@collection';
    var bindVars = { '@collection': this._collection.name() };
    
    if (this._index !== null) {
      query += ' USE INDEX @index';
      bindVars.index = this._index;
    }
    
    var radius = this._radius;
    if (this._distance !== null) {
      radius = this._distance;
    }
    
    query += ' FILTER GEO_DISTANCE([@lat, @lon], doc.@attribute) <= @radius';
    bindVars.lat = this._latitude;
    bindVars.lon = this._longitude;
    bindVars.attribute = this._attribute || 'location';
    bindVars.radius = radius;
    
    if (this._skip !== null && this._skip > 0) {
      query += ' LIMIT @skip';
      bindVars.skip = this._skip;
      if (this._limit !== null && this._limit > 0) {
        query += ', @limit';
        bindVars.limit = this._limit;
      } else {
        query += ', @limit';
        bindVars.limit = 99999999999;
      }
    } else if (this._limit !== null && this._limit > 0) {
      query += ' LIMIT @limit';
      bindVars.limit = this._limit;
    }
    
    query += ' RETURN doc';

    var cursorOptions = {};
    if (this._batchSize !== null) {
      cursorOptions.batchSize = this._batchSize;
    }
    cursorOptions.count = true;

    this._execution = this._collection._database._query(query, bindVars, cursorOptions);

    if (this._execution.hasOwnProperty('count')) {
      this._countQuery = this._execution.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a withinRectangle query
// //////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var query = 'FOR doc IN @@collection';
    var bindVars = { '@collection': this._collection.name() };
    
    if (this._index !== null) {
      query += ' USE INDEX @index';
      bindVars.index = this._index;
    }
    
    query += ' FILTER GEO_CONTAINS([[@lat1, @lon1], [@lat2, @lon2]], doc.@attribute)';
    bindVars.lat1 = this._latitude1;
    bindVars.lon1 = this._longitude1;
    bindVars.lat2 = this._latitude2;
    bindVars.lon2 = this._longitude2;
    bindVars.attribute = this._attribute || 'location';
    
    if (this._skip !== null && this._skip > 0) {
      query += ' LIMIT @skip';
      bindVars.skip = this._skip;
      if (this._limit !== null && this._limit > 0) {
        query += ', @limit';
        bindVars.limit = this._limit;
      } else {
        query += ', @limit';
        bindVars.limit = 99999999999;
      }
    } else if (this._limit !== null && this._limit > 0) {
      query += ' LIMIT @limit';
      bindVars.limit = this._limit;
    }
    
    query += ' RETURN doc';

    var cursorOptions = {};
    if (this._batchSize !== null) {
      cursorOptions.batchSize = this._batchSize;
    }
    cursorOptions.count = true;

    this._execution = this._collection._database._query(query, bindVars, cursorOptions);

    if (this._execution.hasOwnProperty('count')) {
      this._countQuery = this._execution.count;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a fulltext query
// //////////////////////////////////////////////////////////////////////////////
SimpleQueryFulltext.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var query = 'FOR doc IN @@collection';
    var bindVars = { '@collection': this._collection.name() };
    
    if (this._index !== null) {
      query += ' USE INDEX @index';
      bindVars.index = this._index;
    }
    
    query += ' FILTER FULLTEXT(doc, @attribute, @query)';
    bindVars.attribute = this._attribute;
    bindVars.query = this._query;
    
    if (this._skip !== null && this._skip > 0) {
      query += ' LIMIT @skip';
      bindVars.skip = this._skip;
      if (this._limit !== null && this._limit > 0) {
        query += ', @limit';
        bindVars.limit = this._limit;
      } else {
        query += ', @limit';
        bindVars.limit = 99999999999;
      }
    } else if (this._limit !== null && this._limit > 0) {
      query += ' LIMIT @limit';
      bindVars.limit = this._limit;
    }
    
    query += ' RETURN doc';

    var cursorOptions = {};
    if (this._batchSize !== null) {
      cursorOptions.batchSize = this._batchSize;
    }
    cursorOptions.count = true;

    this._execution = this._collection._database._query(query, bindVars, cursorOptions);

    if (this._execution.hasOwnProperty('count')) {
      this._countQuery = this._execution.count;
    }
  }
};

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryArray = SimpleQueryArray;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryByCondition = SimpleQueryByCondition;
exports.SimpleQueryFulltext = SimpleQueryFulltext;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryRange = SimpleQueryRange;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.SimpleQueryWithinRectangle = SimpleQueryWithinRectangle;

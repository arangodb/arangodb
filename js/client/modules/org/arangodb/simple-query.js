/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief Arango Simple Query Language
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangosh = require("arangosh");

var sq = require("simple-query-common");

var GeneralArrayCursor = sq.GeneralArrayCursor;
var SimpleQueryAll = sq.SimpleQueryAll;
var SimpleQueryByExample = sq.SimpleQueryByExample;
var SimpleQueryFulltext = sq.SimpleQueryFulltext;
var SimpleQueryNear = sq.SimpleQueryNear;
var SimpleQueryRange = sq.SimpleQueryRange;
var SimpleQueryWithin = sq.SimpleQueryWithin;

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY ALL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.execute = function (batchSize) {
  var documents;

  if (this._execution === null) {
    if (batchSize != undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection : this._collection.name()
    }  

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }
  
    var requestResult = this._collection._database._connection.PUT("/_api/simple/all", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  QUERY BY EXAMPLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.execute = function (batchSize) {
  var documents;

  if (this._execution === null) {
    if (batchSize != undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }  

    var data = {
      collection : this._collection.name(),
      example : this._example
    }  

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }
    
    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }
  
    var requestResult = this._collection._database._connection.PUT("/_api/simple/by-example", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example for a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.firstExample = function () {
  var example;

  // example is given as only argument
  if (arguments.length == 1) {
    example = arguments[0];
  }

  // example is given as list
  else {
    example = {};

    for (var i = 0;  i < arguments.length;  i += 2) {
      example[arguments[i]] = arguments[i + 1];
    }
  }

  var data = {
    collection : this.name(),
    example : example
  }  

  var requestResult = this._database._connection.PUT("/_api/simple/first-example", JSON.stringify(data));

  if (requestResult !== null
      && requestResult.error == true 
      && requestResult.errorNum == internal.errors.ERROR_HTTP_NOT_FOUND.code) {
    return null;
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.document;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       RANGE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a range query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype.execute = function (batchSize) {
  var documents;

  if (this._execution === null) {
    if (batchSize != undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection : this._collection.name(),
      attribute : this._attribute,
      right : this._right,
      left : this._left,
      closed : this._type == 1
    }  

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }
    
    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }
  
    var requestResult = this._collection._database._connection.PUT("/_api/simple/range", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE QUERY NEAR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.execute = function (batchSize) {
  var documents;

  if (this._execution === null) {
    if (batchSize != undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection : this._collection.name(),
      latitude : this._latitude,
      longitude : this._longitude
    }  

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._index !== null) {
      data.geo = this._index;
    }
  
    if (this._distance !== null) {
      data.distance = this._distance;
    }
    
    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }
  
    var requestResult = this._collection._database._connection.PUT("/_api/simple/near", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               SIMPLE QUERY WITHIN
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.execute = function (batchSize) {
  var documents;

  if (this._execution === null) {
    if (batchSize != undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection : this._collection.name(),
      latitude : this._latitude,
      longitude : this._longitude,
      radius : this._radius
    }  

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._index !== null) {
      data.geo = this._index;
    }
  
    if (this._distance !== null) {
      data.distance = this._distance;
    }
    
    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }
  
    var requestResult = this._collection._database._connection.PUT("/_api/simple/within", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             SIMPLE QUERY FULLTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a fulltext query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype.execute = function (batchSize) {
  var documents;

  if (this._execution === null) {
    if (batchSize != undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection : this._collection.name(),
      attribute : this._attribute,
      query : this._query
    }  
    
    if (this._limit !== null) {
      data.limit = this._limit;
    }
    
    if (this._index !== null) {
      data.index = this._index;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }
  
    var requestResult = this._collection._database._connection.PUT("/_api/simple/fulltext", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup SimpleQuery
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.SimpleQueryFulltext = SimpleQueryFulltext;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

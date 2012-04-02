////////////////////////////////////////////////////////////////////////////////
/// @brief simple queries
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
/// @author Achim Brandt
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("actions");
var simple = require("simple-query");
var API = "_api/simple/";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_all
/// @brief returns all documents of a collection
///
/// @REST{PUT /_api/simple/all}
///
/// Returns all documents of a collections. The call expects a JSON hash array
/// as body with the following attributes:
///
/// @FA{collection}
///
/// The identifier or name of the collection to query.
///
/// @FA{skip} (optional)
///
/// The documents to skip in the query.
///
/// @FA{limit} (optional)
///
/// The maximal amount of documents to return.
///
/// @EXAMPLES
///
/// To get all documents (NEVER DO THAT!)
///
/// @verbinclude api_simple1
///
/// Limit the amount of documents using
///
/// @verbinclude api_simple2
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "all",
  context : "api",

  callback : function (req, res) {
    var body = JSON.parse(req.requestBody || "{}");

    var limit = body.limit;
    var skip = body.skip;
    var name = body.collection;

    if (req.requestType != actions.PUT) {
      actions.resultUnsupported(req, res);
    }
    else {
      collection = db._collection(name);

      if (collection == null) {
        actions.collectionUnknown(req, res, name);
      }
      else {
        var result = collection.all();

        if (skip != null) {
          result = result.skip(skip);
        }

        if (limit != null) {
          result = result.limit(limit);
        }

        actions.resultOk(req, res, actions.HTTP_OK, result.toArray());
      }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_near
/// @brief returns all documents of a collection near a given location
///
/// @REST{PUT /_api/simple/near}
///
/// The default will find at most 100 documents near a given coordinate.  The
/// returned list is sorted according to the distance, with the nearest document
/// coming first. If there are near documents of equal distance, documents are
/// chosen randomly from this set until the limit is reached. It is possible to
/// change the limit using the @FA{limit} operator.
///
/// In order to use the @FN{near} operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the @FN{geo} operator to select a particular index.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// @FA{collection}
///
/// The identifier or name of the collection to query.
///
/// @FA{latitude}
///
/// The latitude of the coordinate.
///
/// @FA{longitude}
///
/// The longitude of the coordinate.
///
/// @FA{distance} (optional)
///
/// If given, the attribute key used to store the distance.
///
/// @FA{skip} (optional)
///
/// The documents to skip in the query.
///
/// @FA{limit} (optional)
///
/// The maximal amount of documents to return.
///
/// @FA{geo} (optional)
///
/// If given, the identifier of the geo-index to use.
///
/// @EXAMPLES
///
/// Without distance:
///
/// @verbinclude api_simple3
///
/// With distance:
///
/// @verbinclude api_simple4
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "near",
  context : "api",

  callback : function (req, res) {
    var body = JSON.parse(req.requestBody || "{}");

    var limit = body.limit;
    var skip = body.skip;
    var latitude = body.latitude;
    var longitude = body.longitude;
    var distance = body.distance;
    var name = body.collection;
    var geo = body.geo;

    if (req.requestType != actions.PUT) {
      actions.unsupported(req, res);
    }
    else {
      collection = db._collection(name);

      if (collection == null) {
        actions.collectionUnknown(req, res, name);
      }
      else if (latitude == null) {
        actions.badParameter(req, res, "latitude");
      }
      else if (longitude == null) {
        actions.badParameter(req, res, "longitude");
      }
      else {
        var result;

        if (geo == null) {
          result = collection.near(latitude, longitude);
        }
        else {
          result = collection.geo(geo).near(latitude, longitude);
        }

        if (skip != null) {
          result = result.skip(skip);
        }

        if (limit != null) {
          result = result.limit(limit);
        }

        if (distance != null) {
          result = result.distance(distance);
        }

        actions.resultOk(req, res, actions.HTTP_OK, result.toArray());
      }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_within
/// @brief returns all documents of a collection within a given radius
///
/// @REST{PUT /_api/simple/within}
///
/// This will find all documents with in a given radius around the coordinate
/// (@FA{latitude}, @FA{longitude}). The returned list is sorted by distance.
///
/// In order to use the @FN{within} operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the @FN{geo} operator to select a particular index.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// @FA{collection}
///
/// The identifier or name of the collection to query.
///
/// @FA{latitude}
///
/// The latitude of the coordinate.
///
/// @FA{longitude}
///
/// The longitude of the coordinate.
///
/// @FA{radius}
///
/// The maximal radius.
///
/// @FA{distance} (optional)
///
/// If given, the attribute key used to store the distance.
///
/// @FA{skip} (optional)
///
/// The documents to skip in the query.
///
/// @FA{limit} (optional)
///
/// The maximal amount of documents to return.
///
/// @FA{geo} (optional)
///
/// If given, the identifier of the geo-index to use.
///
/// @EXAMPLES
///
/// Without distance:
///
/// @verbinclude api_simple5
///
/// With distance:
///
/// @verbinclude api_simple6
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "within",
  context : "api",

  callback : function (req, res) {
    var body = JSON.parse(req.requestBody || "{}");

    var limit = body.limit;
    var skip = body.skip;
    var latitude = body.latitude;
    var longitude = body.longitude;
    var distance = body.distance;
    var radius = body.radius;
    var name = body.collection;
    var geo = body.geo;

    if (req.requestType != actions.PUT) {
      actions.unsupported(req, res);
    }
    else {
      collection = db._collection(name);

      if (collection == null) {
        actions.collectionUnknown(req, res, name);
      }
      else if (latitude == null) {
        actions.badParameter(req, res, "latitude");
      }
      else if (longitude == null) {
        actions.badParameter(req, res, "longitude");
      }
      else {
        var result;

        if (geo == null) {
          result = collection.within(latitude, longitude, radius);
        }
        else {
          result = collection.geo(geo).within(latitude, longitude, radius);
        }

        if (skip != null) {
          result = result.skip(skip);
        }

        if (limit != null) {
          result = result.limit(limit);
        }

        if (distance != null) {
          result = result.distance(distance);
        }

        actions.resultOk(req, res, actions.HTTP_OK, result.toArray());
      }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_by_example
/// @brief returns all documents of a collection matching a given example
///
/// @REST{PUT /_api/simple/by-example}
///
/// This will find all documents matching a given example.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// @FA{collection}
///
/// The identifier or name of the collection to query.
///
/// @FA{example}
///
/// The example.
///
/// @FA{skip} (optional)
///
/// The documents to skip in the query.
///
/// @FA{limit} (optional)
///
/// The maximal amount of documents to return.
///
/// @EXAMPLES
///
/// @verbinclude api_simple7
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "by-example",
  context : "api",

  callback : function (req, res) {
    var body = JSON.parse(req.requestBody || "{}");

    var limit = body.limit;
    var skip = body.skip;
    var name = body.collection;
    var example = body.example;

    if (req.requestType != actions.PUT) {
      actions.unsupported(req, res);
    }
    else {
      collection = db._collection(name);

      if (collection == null) {
        actions.collectionUnknown(req, res, name);
      }
      else if (typeof example !== "object") {
        actions.badParameter(req, res, "example");
      }
      else {
        var result = collection.byExample(example);

        if (skip != null) {
          result = result.skip(skip);
        }

        if (limit != null) {
          result = result.limit(limit);
        }

        actions.resultOk(req, res, actions.HTTP_OK, result.toArray());
      }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

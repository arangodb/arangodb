/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Graph, arguments */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Florian Bartels
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


var arangodb = require("org/arangodb"),
	ArangoCollection = arangodb.ArangoCollection,
	db = arangodb.db;


// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/general-graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief transform a string into an array.
////////////////////////////////////////////////////////////////////////////////


var stringToArray = function (x) {
	if (typeof(x) === "string") {
		return [x];
	}
	return x;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a parameter is not defined, an empty string or an empty
//  array
////////////////////////////////////////////////////////////////////////////////


var isValidCollectionsParameter = function (x) {
	if (!x) {
		return false;
	}
	if (Array.isArray(x) && x.length === 0) {
		return false;
	}
	if (typeof(x) !== "string" && !Array.isArray(x)) {
		return false;
	}
	return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create a collection by name
////////////////////////////////////////////////////////////////////////////////

var findOrCreateCollectionByName = function (name, type) {
	var col = db._collection(name),res = false;
	if (col === null) {
		if (type === ArangoCollection.TYPE_DOCUMENT) {
			col = db._create(name);
		} else {
			col = db._createEdgeCollection(name);
		}
		res = true;
	} else if (!(col instanceof ArangoCollection) || col.type() !== type) {
		throw "<" + name + "> must be a " +
			(type === ArangoCollection.TYPE_DOCUMENT ? "document" : "edge")
		+ " collection";
	}
	return res;
};

// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/general-graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief define an undirected relation.
////////////////////////////////////////////////////////////////////////////////


var _undirectedRelationDefinition = function (relationName, vertexCollections) {

	if (arguments.length < 2) {
		throw "method _undirectedRelationDefinition expects 2 arguments";
	}

	if (typeof relationName !== "string" || relationName === "") {
		throw "<relationName> must be a not empty string";
	}

	if (!isValidCollectionsParameter(vertexCollections)) {
		throw "<vertexCollections> must be a not empty string or array";
	}

	return {
		collection: "relationName",
		from: stringToArray(vertexCollections),
		to: stringToArray(vertexCollections)
	};
};


////////////////////////////////////////////////////////////////////////////////
/// @brief define an directed relation.
////////////////////////////////////////////////////////////////////////////////


var _directedRelationDefinition = function (
	relationName, fromVertexCollections, toVertexCollections) {

	if (arguments.length < 3) {
		throw "method _undirectedRelationDefinition expects 3 arguments";
	}

	if (typeof relationName !== "string" || relationName === "") {
		throw "<relationName> must be a not empty string";
	}

	if (!isValidCollectionsParameter(fromVertexCollections)) {
		throw "<fromVertexCollections> must be a not empty string or array";
	}

	if (!isValidCollectionsParameter(toVertexCollections)) {
		throw "<toVertexCollections> must be a not empty string or array";
	}

	return {
		collection: "relationName",
		from: stringToArray(fromVertexCollections),
		to: stringToArray(toVertexCollections)
	};
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list of edge definitions
////////////////////////////////////////////////////////////////////////////////


var edgeDefinitions = function () {

	var res = [], args = arguments;
	Object.keys(args).forEach(function (x) {
		res.push(args[x]);
	});

	return res;

};

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new graph
////////////////////////////////////////////////////////////////////////////////


var _create = function (graphName, edgeDefinitions) {

	var gdb = db._collection("_graphs"),
		g,
		graphAlreadyExists = true,
		createdCollections = []
	;

	if (gdb === null) {
		throw "_graphs collection does not exist.";
	}

	if (!graphName) {
		throw "a graph name is required to create a graph.";
	}
	if (!Array.isArray(edgeDefinitions) || edgeDefinitions.length === 0) {
		throw "at least one edge definition is required to create a graph.";
	}

	try {
		g = gdb.document(graphName);
	} catch (e) {
		if (e.errorNum !== 1202) {
			throw e;
		}
		graphAlreadyExists = false;
	}

	if (graphAlreadyExists) {
		throw "graph " + graphName + " already exists.";
	}

	edgeDefinitions.forEach(function (e) {
		e.from.concat(e.to).forEach(function (v) {
			if (findOrCreateCollectionByName(v, ArangoCollection.TYPE_DOCUMENT)) {
				createdCollections.push(v);
			}
		});
		if (findOrCreateCollectionByName(e.collection, ArangoCollection.TYPE_EDGE)) {
			createdCollections.push(e.collection);
		}
	});

	gdb.save({
		'edgeDefinitions' : edgeDefinitions,
		'_key' : graphName
	});

	return new Graph(graphName, edgeDefinitions);

};



////////////////////////////////////////////////////////////////////////////////
/// @brief load a graph.
////////////////////////////////////////////////////////////////////////////////

var Graph = function(graphName, edgeDefinitions) {

};

var _graph = function() {
  return new Graph();
};

Graph.prototype.edgeCollections = function() {
  //testdummy
  return [require("internal").db.w3];
};

Graph.prototype.vertexCollections = function() {
  //testdummy
  return [require("internal").db.a, require("internal").db.b];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief edges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.edges = function(vertexId) {
  var edgeCollections = this.edgeCollections();
  var result = [];


  edgeCollections.forEach(
    function(edgeCollection) {
      //todo: test, if collection may point to vertex
      result = result.concat(edgeCollection.edges(vertexId));
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.inEdges = function(vertexId) {
  var edgeCollections = this.edgeCollections();
  var result = [];


  edgeCollections.forEach(
    function(edgeCollection) {
      //todo: test, if collection may point to vertex
      result = result.concat(edgeCollection.inEdges(vertexId));
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.outEdges = function(vertexId) {
  var edgeCollections = this.edgeCollections();
  var result = [];


  edgeCollections.forEach(
    function(edgeCollection) {
      //todo: test, if collection may point to vertex
      result = result.concat(edgeCollection.outEdges(vertexId));
    }
  );
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get ingoing vertex of an edge.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getInVertex = function(edgeId) {
  var edgeCollection = this.getEdgeCollectionByName(edgeId.split("/")[0]);
  var document = edgeCollection.document(edgeId);
  if (document) {
    var vertexId = document._from;
    var vertexCollection = this.getVertexCollectionByName(vertexId.split("/")[0]);
    return vertexCollection.document(vertexId);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get outgoing vertex of an edge .
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getOutVertex = function(edgeId) {
  var edgeCollection = this.getEdgeCollectionByName(edgeId.split("/")[0]);
  var document = edgeCollection.document(edgeId);
  if (document) {
    var vertexId = document._to;
    var vertexCollection = this.getVertexCollectionByName(vertexId.split("/")[0]);
    return vertexCollection.document(vertexId);
  }
};


////////////////////////////////////////////////////////////////////////////////
/// @brief get edge collection by name.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdgeCollectionByName = function(name) {
  var edgeCollections = this.edgeCollections();
  var results = edgeCollections.filter(
    function(edgeCollection) {
      return edgeCollection.name() === name;
    }
  );

  if (results.length === 1) {
    return results[0];
  }
  throw "Collection " + name + " does not exist in graph.";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get vertex collection by name.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertexCollectionByName = function(name) {
  var vertexCollections = this.vertexCollections();
  var results = vertexCollections.filter(
    function(vertexCollection) {
      return vertexCollection.name() === name;
    }
  );

  if (results.length === 1) {
    return results[0];
  }
  throw "Collection " + name + " does not exist in graph.";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports._undirectedRelationDefinition = _undirectedRelationDefinition;
exports._directedRelationDefinition = _directedRelationDefinition;
exports._graph = _graph;
exports.edgeDefinitions = edgeDefinitions;
exports._create = _create;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

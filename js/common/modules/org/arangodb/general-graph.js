/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

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


var collection = require("org/arangodb/arango-collection");


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

	if (typeof x === "String") {
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
	if (typeof x !== "String" && !Array.isArray(x)) {
		return false;
	}
	return true;
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

	if (_undirectedRelationDefinition.arguments.length < 2) {
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


var _directedRelationDefinition = function (relationName, fromVertexCollections, toVertexCollections) {

	if (_directedRelationDefinition.arguments.length < 3) {
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





// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports._undirectedRelationDefinition = _undirectedRelationDefinition;
exports._directedRelationDefinition = _directedRelationDefinition;



// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

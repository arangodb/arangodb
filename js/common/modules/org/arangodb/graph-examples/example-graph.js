/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Graph, arguments */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph Data for Example
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
/// @author Michael Hackstein
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
(function() {

  var Graph = require("org/arangodb/general-graph");

  var createTraversalExample = function() {
    var g = Graph._create("knows_graph",
      [Graph._undirectedRelationDefinition("knows", "persons")]
    );
    var a = g.persons.save({name: "Alice", _key: "alice"})._id;
    var b = g.persons.save({name: "Bob", _key: "bob"})._id;
    var c = g.persons.save({name: "Charlie", _key: "charlie"})._id;
    var d = g.persons.save({name: "Dave", _key: "dave"})._id;
    var e = g.persons.save({name: "Eve", _key: "eve"})._id;
    g.knows.save(a, b, {});
    g.knows.save(b, c, {});
    g.knows.save(b, d, {});
    g.knows.save(e, a, {});
    g.knows.save(e, b, {});
    return g;
  };

  var createSocialGraph = function() {
    var edgeDefinition = [];
    edgeDefinition.push(Graph._undirectedRelationDefinition("relation", ["female", "male"]));
    var g = Graph._create("social", edgeDefinition);
    var a = g.female.save({name: "Alice", _key: "alice"});
    var b = g.male.save({name: "Bob", _key: "bob"});
    var c = g.male.save({name: "Charly", _key: "charly"});
    var d = g.female.save({name: "Diana", _key: "diana"});
    g.relation.save(a._id, b._id, {type: "married", _key: "aliceAndBob"});
    g.relation.save(a._id, c._id, {type: "friend", _key: "aliceAndCharly"});
    g.relation.save(c._id, d._id, {type: "married", _key: "charlyAndDiana"});
    g.relation.save(b._id, d._id, {type: "friend", _key: "bobAndDiana"});
    return g;
  };

  var createRoutePlannerGraph = function() {
    var edgeDefinition = [];
    edgeDefinition.push(Graph._directedRelationDefinition("highway", ["city"], ["city"]));
    edgeDefinition.push(Graph._directedRelationDefinition(
      "road", ["village", "city"], ["village", "city"])
    );
    var g = Graph._create("routeplanner", edgeDefinition);
    var berlin = g.city.save({_key: "Berlin", population : 3000000, isCapital : true});
    var cologne = g.city.save({_key: "Cologne", population : 1000000, isCapital : false});
    var munich = g.city.save({_key: "Munich", population : 1000000, isCapital : true});
    var olpe = g.village.save({_key: "Olpe", population : 80000, isCapital : false});
    var rosenheim = g.village.save({_key: "Rosenheim", population : 80000, isCapital : false});
    g.highway.save(berlin._id, cologne._id, {distance: 850});
    g.highway.save(berlin._id, munich._id, {distance: 600});
    g.highway.save(munich._id, cologne._id, {distance: 650});
    g.road.save(berlin._id, olpe._id, {distance: 700});
    g.road.save(berlin._id, rosenheim._id, {distance: 800});
    g.road.save(munich._id, rosenheim._id, {distance: 80});
    g.road.save(munich._id, olpe._id, {distance: 600});
    g.road.save(cologne._id, olpe._id, {distance: 100});
    g.road.save(cologne._id, rosenheim._id, {distance: 750});
    return g;
  };


  var dropGraph = function(name) {
    if (Graph._exists(name)) {
      return Graph._drop(name, true);
    }
  };

  var loadGraph = function(name) {
    switch (name) {
      case "knows_graph":
        return createTraversalExample();
      case "routeplanner":
        return createRoutePlannerGraph();
      case "social":
        return createSocialGraph();
    }

  };

  exports.loadGraph = loadGraph;
  exports.dropGraph = dropGraph;
}());

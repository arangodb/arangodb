/*jshint strict: false */
/*global require, exports */

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

var Graph = require("org/arangodb/general-graph");

var createTraversalExample = function() {
  var g = Graph._create("knows_graph",
    [Graph._relation("knows", "persons", "persons")]
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
  edgeDefinition.push(Graph._relation("relation", ["female", "male"], ["female", "male"]));
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
  edgeDefinition.push(Graph._relation(
    "germanHighway", ["germanCity"], ["germanCity"])
  );
  edgeDefinition.push(
    Graph._relation("frenchHighway", ["frenchCity"], ["frenchCity"])
  );
  edgeDefinition.push(Graph._relation(
    "internationalHighway", ["frenchCity", "germanCity"], ["frenchCity", "germanCity"])
  );
  var g = Graph._create("routeplanner", edgeDefinition);
  var berlin = g.germanCity.save({_key: "Berlin", population : 3000000, isCapital : true});
  var cologne = g.germanCity.save({_key: "Cologne", population : 1000000, isCapital : false});
  var hamburg = g.germanCity.save({_key: "Hamburg", population : 1000000, isCapital : false});
  var lyon = g.frenchCity.save({_key: "Lyon", population : 80000, isCapital : false});
  var paris = g.frenchCity.save({_key: "Paris", population : 4000000, isCapital : true});
  g.germanHighway.save(berlin._id, cologne._id, {distance: 850});
  g.germanHighway.save(berlin._id, hamburg._id, {distance: 400});
  g.germanHighway.save(hamburg._id, cologne._id, {distance: 500});
  g.frenchHighway.save(paris._id, lyon._id, {distance: 550});
  g.internationalHighway.save(berlin._id, lyon._id, {distance: 1100});
  g.internationalHighway.save(berlin._id, paris._id, {distance: 1200});
  g.internationalHighway.save(hamburg._id, paris._id, {distance: 900});
  g.internationalHighway.save(hamburg._id, lyon._id, {distance: 1300});
  g.internationalHighway.save(cologne._id, lyon._id, {distance: 700});
  g.internationalHighway.save(cologne._id, paris._id, {distance: 550});
  return g;
};


var dropGraph = function(name) {
  if (Graph._exists(name)) {
    return Graph._drop(name, true);
  }
};

var loadGraph = function(name) {
  dropGraph(name);
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief Graph Data for Example
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
// / @author Michael Hackstein
// / @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var Graph = require('@arangodb/general-graph');
var db = require('internal').db;

var arangodb = require('@arangodb');
var ArangoError = arangodb.ArangoError;

// we create a graph with 'knows' pointing from 'persons' to 'persons'
var createTraversalExample = function () {
  var g = Graph._create('knows_graph',
    [Graph._relation('knows', 'persons', 'persons')]
  );
  var a = g.persons.save({name: 'Alice', _key: 'alice'});
  var b = g.persons.save({name: 'Bob', _key: 'bob'});
  var c = g.persons.save({name: 'Charlie', _key: 'charlie'});
  var d = g.persons.save({name: 'Dave', _key: 'dave'});
  var e = g.persons.save({name: 'Eve', _key: 'eve'});
  g.knows.save(a._id, b._id, {vertex:a._key});
  g.knows.save(b._id, c._id, {vertex:b._key});
  g.knows.save(b._id, d._id, {vertex:b._key});
  g.knows.save(e._id, a._id, {vertex:e._key});
  g.knows.save(e._id, b._id, {vertex:e._key});
  return g;
};

// we create a graph with 'edges2' pointing from 'verts' to 'verts'
var createMpsTraversal = function () {
  var g = Graph._create('mps_graph',
    [Graph._relation('mps_edges', 'mps_verts', 'mps_verts')]
  );  
  var a = g.mps_verts.save({_key: 'A'});
  var b = g.mps_verts.save({_key: 'B'});
  var c = g.mps_verts.save({_key: 'C'});
  var d = g.mps_verts.save({_key: 'D'});
  var e = g.mps_verts.save({_key: 'E'});
  var f = g.mps_verts.save({_key: 'F'});
  g.mps_edges.save(a._id, b._id, {vertex:a._key});
  g.mps_edges.save(a._id, e._id, {vertex:a._key});
  g.mps_edges.save(a._id, d._id, {vertex:a._key});
  g.mps_edges.save(b._id, c._id, {vertex:b._key});
  g.mps_edges.save(d._id, c._id, {vertex:d._key});
  g.mps_edges.save(e._id, f._id, {vertex:e._key});
  g.mps_edges.save(f._id, c._id, {vertex:f._key});
  return g;
};


// we create a graph with 'relation' pointing from 'female' to 'male' and 'male
var createSocialGraph = function () {
  db._create("female");
  db._create("male", {distributeShardsLike:"female"});
  db._createEdgeCollection("relation", {
                            shardKeys:["vertex"],
                            distributeShardsLike:"female"});

  var edgeDefinition = [];
  edgeDefinition.push(Graph._relation('relation', ['female', 'male'], ['female', 'male']));
  var g = Graph._create('social', edgeDefinition);
  var a = g.female.save({name: 'Alice', _key: 'alice'});
  var b = g.male.save({name: 'Bob', _key: 'bob'});
  var c = g.male.save({name: 'Charly', _key: 'charly'});
  var d = g.female.save({name: 'Diana', _key: 'diana'});
  g.relation.save(a._id, b._id, {type: 'married', vertex:a._key});
  g.relation.save(a._id, c._id, {type: 'friend', vertex:a._key});
  g.relation.save(c._id, d._id, {type: 'married', vertex:c._key});
  g.relation.save(b._id, d._id, {type: 'friend', vertex:b._key});
  return g;
};

var createRoutePlannerGraph = function () {
  var edgeDefinition = [];
  edgeDefinition.push(Graph._relation(
    'germanHighway', ['germanCity'], ['germanCity'])
  );
  edgeDefinition.push(
    Graph._relation('frenchHighway', ['frenchCity'], ['frenchCity'])
  );
  edgeDefinition.push(Graph._relation(
    'internationalHighway', ['frenchCity', 'germanCity'], ['frenchCity', 'germanCity'])
  );

  var g = Graph._create('routeplanner', edgeDefinition);
  var berlin = g.germanCity.save({
    _key: 'Berlin',
    population: 3000000,
    isCapital: true,
    geometry: {
      'type': 'Point',
      'coordinates': [13.3833, 52.5167]
    }});
  var cologne = g.germanCity.save({
    _key: 'Cologne',
    population: 1000000,
    isCapital: false,
    geometry: {
      'type': 'Point',
      'coordinates': [6.9528, 50.9364]
    }});
  var hamburg = g.germanCity.save({
    _key: 'Hamburg',
    population: 1000000,
    isCapital: false,
    geometry: {
      'type': 'Point',
      'coordinates': [10.0014, 53.5653]
    }});
  var lyon = g.frenchCity.save({
    _key: 'Lyon',
    population: 80000,
    isCapital: false,
    geometry: {
      'type': 'Point',
      'coordinates': [4.8400, 45.7600]
    }});
  var paris = g.frenchCity.save({
    _key: 'Paris',
    population: 4000000,
    isCapital: true,
    geometry: {
      'type': 'Point',
      'coordinates': [2.3508, 48.8567]
    }});
  g.germanCity.ensureIndex({ type: "geo", fields: [ "geometry" ], geoJson:true });
  g.frenchCity.ensureIndex({ type: "geo", fields: [ "geometry" ], geoJson:true });
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

var saveWorldCountryGraphData = function () {
  // This example writes the graph data without using the general graph interface.
  // it expects to find the collections to be already created.
  // It can be called either via createWorldCountryGraph() that will create a managed graph
  // or via dropWorldCountryUnManaged that will simply generate a set of vertex and edge collections.
  // instead of using 
  //    var world  = worldCountryGraph.worldVertices.save({ _key: "world", name: "World", type: "root" })
  //    var africa = worldCountryGraph.worldVertices.save({ _key: "continent-africa", name: "Africa", type: "continent" })
  //    worldCountryGraph.worldEdges.save(world._id, africa._id, { type: "is-in" })
  //
  // we will directly put the documents into their respective collections
  // and have to ensure graph consistency by ourselves.

  // vertices: root node 
  var db = require('internal').db;
  db.worldVertices.save({ _key: 'world', name: 'World', type: 'root' });

  // vertices: continents 
  db.worldVertices.save({ _key: 'continent-africa', name: 'Africa', type: 'continent' });
  db.worldVertices.save({ _key: 'continent-asia', name: 'Asia', type: 'continent' });
  db.worldVertices.save({ _key: 'continent-australia', name: 'Australia', type: 'continent' });
  db.worldVertices.save({ _key: 'continent-europe', name: 'Europe', type: 'continent' });
  db.worldVertices.save({ _key: 'continent-north-america', name: 'North America', type: 'continent' });
  db.worldVertices.save({ _key: 'continent-south-america', name: 'South America', type: 'continent' });

  // vertices: countries 
  db.worldVertices.save({ _key: 'country-afghanistan', name: 'Afghanistan', type: 'country', code: 'AFG' });
  db.worldVertices.save({ _key: 'country-albania', name: 'Albania', type: 'country', code: 'ALB' });
  db.worldVertices.save({ _key: 'country-algeria', name: 'Algeria', type: 'country', code: 'DZA' });
  db.worldVertices.save({ _key: 'country-andorra', name: 'Andorra', type: 'country', code: 'AND' });
  db.worldVertices.save({ _key: 'country-angola', name: 'Angola', type: 'country', code: 'AGO' });
  db.worldVertices.save({ _key: 'country-antigua-and-barbuda', name: 'Antigua and Barbuda', type: 'country', code: 'ATG' });
  db.worldVertices.save({ _key: 'country-argentina', name: 'Argentina', type: 'country', code: 'ARG' });
  db.worldVertices.save({ _key: 'country-australia', name: 'Australia', type: 'country', code: 'AUS' });
  db.worldVertices.save({ _key: 'country-austria', name: 'Austria', type: 'country', code: 'AUT' });
  db.worldVertices.save({ _key: 'country-bahamas', name: 'Bahamas', type: 'country', code: 'BHS' });
  db.worldVertices.save({ _key: 'country-bahrain', name: 'Bahrain', type: 'country', code: 'BHR' });
  db.worldVertices.save({ _key: 'country-bangladesh', name: 'Bangladesh', type: 'country', code: 'BGD' });
  db.worldVertices.save({ _key: 'country-barbados', name: 'Barbados', type: 'country', code: 'BRB' });
  db.worldVertices.save({ _key: 'country-belgium', name: 'Belgium', type: 'country', code: 'BEL' });
  db.worldVertices.save({ _key: 'country-bhutan', name: 'Bhutan', type: 'country', code: 'BTN' });
  db.worldVertices.save({ _key: 'country-bolivia', name: 'Bolivia', type: 'country', code: 'BOL' });
  db.worldVertices.save({ _key: 'country-bosnia-and-herzegovina', name: 'Bosnia and Herzegovina', type: 'country', code: 'BIH' });
  db.worldVertices.save({ _key: 'country-botswana', name: 'Botswana', type: 'country', code: 'BWA' });
  db.worldVertices.save({ _key: 'country-brazil', name: 'Brazil', type: 'country', code: 'BRA' });
  db.worldVertices.save({ _key: 'country-brunei', name: 'Brunei', type: 'country', code: 'BRN' });
  db.worldVertices.save({ _key: 'country-bulgaria', name: 'Bulgaria', type: 'country', code: 'BGR' });
  db.worldVertices.save({ _key: 'country-burkina-faso', name: 'Burkina Faso', type: 'country', code: 'BFA' });
  db.worldVertices.save({ _key: 'country-burundi', name: 'Burundi', type: 'country', code: 'BDI' });
  db.worldVertices.save({ _key: 'country-cambodia', name: 'Cambodia', type: 'country', code: 'KHM' });
  db.worldVertices.save({ _key: 'country-cameroon', name: 'Cameroon', type: 'country', code: 'CMR' });
  db.worldVertices.save({ _key: 'country-canada', name: 'Canada', type: 'country', code: 'CAN' });
  db.worldVertices.save({ _key: 'country-chad', name: 'Chad', type: 'country', code: 'TCD' });
  db.worldVertices.save({ _key: 'country-chile', name: 'Chile', type: 'country', code: 'CHL' });
  db.worldVertices.save({ _key: 'country-colombia', name: 'Colombia', type: 'country', code: 'COL' });
  db.worldVertices.save({ _key: 'country-cote-d-ivoire', name: "Cote d'Ivoire", type: 'country', code: 'CIV' });
  db.worldVertices.save({ _key: 'country-croatia', name: 'Croatia', type: 'country', code: 'HRV' });
  db.worldVertices.save({ _key: 'country-czech-republic', name: 'Czech Republic', type: 'country', code: 'CZE' });
  db.worldVertices.save({ _key: 'country-denmark', name: 'Denmark', type: 'country', code: 'DNK' });
  db.worldVertices.save({ _key: 'country-ecuador', name: 'Ecuador', type: 'country', code: 'ECU' });
  db.worldVertices.save({ _key: 'country-egypt', name: 'Egypt', type: 'country', code: 'EGY' });
  db.worldVertices.save({ _key: 'country-eritrea', name: 'Eritrea', type: 'country', code: 'ERI' });
  db.worldVertices.save({ _key: 'country-finland', name: 'Finland', type: 'country', code: 'FIN' });
  db.worldVertices.save({ _key: 'country-france', name: 'France', type: 'country', code: 'FRA' });
  db.worldVertices.save({ _key: 'country-germany', name: 'Germany', type: 'country', code: 'DEU' });
  db.worldVertices.save({ _key: 'country-people-s-republic-of-china', name: "People's Republic of China", type: 'country', code: 'CHN' });

  // vertices: capitals 
  db.worldVertices.save({ _key: 'capital-algiers', name: 'Algiers', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-andorra-la-vella', name: 'Andorra la Vella', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-asmara', name: 'Asmara', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-bandar-seri-begawan', name: 'Bandar Seri Begawan', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-beijing', name: 'Beijing', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-berlin', name: 'Berlin', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-bogota', name: 'Bogota', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-brasilia', name: 'Brasilia', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-bridgetown', name: 'Bridgetown', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-brussels', name: 'Brussels', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-buenos-aires', name: 'Buenos Aires', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-bujumbura', name: 'Bujumbura', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-cairo', name: 'Cairo', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-canberra', name: 'Canberra', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-copenhagen', name: 'Copenhagen', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-dhaka', name: 'Dhaka', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-gaborone', name: 'Gaborone', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-helsinki', name: 'Helsinki', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-kabul', name: 'Kabul', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-la-paz', name: 'La Paz', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-luanda', name: 'Luanda', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-manama', name: 'Manama', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-nassau', name: 'Nassau', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-n-djamena', name: "N'Djamena", type: 'capital' });
  db.worldVertices.save({ _key: 'capital-ottawa', name: 'Ottawa', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-ouagadougou', name: 'Ouagadougou', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-paris', name: 'Paris', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-phnom-penh', name: 'Phnom Penh', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-prague', name: 'Prague', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-quito', name: 'Quito', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-saint-john-s', name: "Saint John's", type: 'capital' });
  db.worldVertices.save({ _key: 'capital-santiago', name: 'Santiago', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-sarajevo', name: 'Sarajevo', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-sofia', name: 'Sofia', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-thimphu', name: 'Thimphu', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-tirana', name: 'Tirana', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-vienna', name: 'Vienna', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-yamoussoukro', name: 'Yamoussoukro', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-yaounde', name: 'Yaounde', type: 'capital' });
  db.worldVertices.save({ _key: 'capital-zagreb', name: 'Zagreb', type: 'capital' });

  // edges: continent -> world 
  db.worldEdges.save('worldVertices/continent-africa', 'worldVertices/world', { type: 'is-in' });
  db.worldEdges.save('worldVertices/continent-asia', 'worldVertices/world', { type: 'is-in' });
  db.worldEdges.save('worldVertices/continent-australia', 'worldVertices/world', { type: 'is-in' });
  db.worldEdges.save('worldVertices/continent-europe', 'worldVertices/world', { type: 'is-in' });
  db.worldEdges.save('worldVertices/continent-north-america', 'worldVertices/world', { type: 'is-in' });
  db.worldEdges.save('worldVertices/continent-south-america', 'worldVertices/world', { type: 'is-in' });

  // edges: country -> continent 
  db.worldEdges.save('worldVertices/country-afghanistan', 'worldVertices/continent-asia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-albania', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-algeria', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-andorra', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-angola', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-antigua-and-barbuda', 'worldVertices/continent-north-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-argentina', 'worldVertices/continent-south-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-australia', 'worldVertices/continent-australia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-austria', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-bahamas', 'worldVertices/continent-north-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-bahrain', 'worldVertices/continent-asia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-bangladesh', 'worldVertices/continent-asia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-barbados', 'worldVertices/continent-north-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-belgium', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-bhutan', 'worldVertices/continent-asia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-bolivia', 'worldVertices/continent-south-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-bosnia-and-herzegovina', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-botswana', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-brazil', 'worldVertices/continent-south-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-brunei', 'worldVertices/continent-asia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-bulgaria', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-burkina-faso', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-burundi', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-cambodia', 'worldVertices/continent-asia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-cameroon', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-canada', 'worldVertices/continent-north-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-chad', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-chile', 'worldVertices/continent-south-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-colombia', 'worldVertices/continent-south-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-cote-d-ivoire', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-croatia', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-czech-republic', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-denmark', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-ecuador', 'worldVertices/continent-south-america', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-egypt', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-eritrea', 'worldVertices/continent-africa', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-finland', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-france', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-germany', 'worldVertices/continent-europe', { type: 'is-in' });
  db.worldEdges.save('worldVertices/country-people-s-republic-of-china', 'worldVertices/continent-asia', { type: 'is-in' });

  // edges: capital -> country 
  db.worldEdges.save('worldVertices/capital-algiers', 'worldVertices/country-algeria', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-andorra-la-vella', 'worldVertices/country-andorra', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-asmara', 'worldVertices/country-eritrea', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-bandar-seri-begawan', 'worldVertices/country-brunei', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-beijing', 'worldVertices/country-people-s-republic-of-china', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-berlin', 'worldVertices/country-germany', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-bogota', 'worldVertices/country-colombia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-brasilia', 'worldVertices/country-brazil', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-bridgetown', 'worldVertices/country-barbados', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-brussels', 'worldVertices/country-belgium', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-buenos-aires', 'worldVertices/country-argentina', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-bujumbura', 'worldVertices/country-burundi', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-cairo', 'worldVertices/country-egypt', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-canberra', 'worldVertices/country-australia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-copenhagen', 'worldVertices/country-denmark', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-dhaka', 'worldVertices/country-bangladesh', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-gaborone', 'worldVertices/country-botswana', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-helsinki', 'worldVertices/country-finland', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-kabul', 'worldVertices/country-afghanistan', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-la-paz', 'worldVertices/country-bolivia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-luanda', 'worldVertices/country-angola', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-manama', 'worldVertices/country-bahrain', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-nassau', 'worldVertices/country-bahamas', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-n-djamena', 'worldVertices/country-chad', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-ottawa', 'worldVertices/country-canada', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-ouagadougou', 'worldVertices/country-burkina-faso', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-paris', 'worldVertices/country-france', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-phnom-penh', 'worldVertices/country-cambodia', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-prague', 'worldVertices/country-czech-republic', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-quito', 'worldVertices/country-ecuador', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-saint-john-s', 'worldVertices/country-antigua-and-barbuda', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-santiago', 'worldVertices/country-chile', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-sarajevo', 'worldVertices/country-bosnia-and-herzegovina', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-sofia', 'worldVertices/country-bulgaria', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-thimphu', 'worldVertices/country-bhutan', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-tirana', 'worldVertices/country-albania', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-vienna', 'worldVertices/country-austria', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-yamoussoukro', 'worldVertices/country-cote-d-ivoire', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-yaounde', 'worldVertices/country-cameroon', { type: 'is-in' });
  db.worldEdges.save('worldVertices/capital-zagreb', 'worldVertices/country-croatia', { type: 'is-in' });
};

var createWorldCountryGraph = function () {

  // Here we use the graph module to create the collections for the world example:
  /* jshint ignore:start */
  var worldCountryGraph = Graph._create('worldCountry',
    [Graph._relation('worldEdges', ['worldVertices'], ['worldVertices'])]
  );
  /* jshint ignore:end */
  // and load the data of the graph into the collections created by the graph module:
  saveWorldCountryGraphData();
  return worldCountryGraph;
};

var createWorldCountryGraphUnManaged = function () {
  // if we don't manage the graph, we need to drop the vertex/edge collections manually.
  var db = require('internal').db;
  db._create('worldVertices');
  db._createEdgeCollection('worldEdges');
  saveWorldCountryGraphData();
};

var dropWorldCountryUnManaged = function () {

  // here we just use flat vertex/edge collections
  var db = require('internal').db;
  db._drop('worldVertices');
  db._drop('worldEdges');
};

var createTraversalGraph = function () {
  var graph_module = require('@arangodb/general-graph');

  var graph = graph_module._create('traversalGraph', [
    graph_module._relation('edges', 'circles', ['circles'])]);

  // Add circle circles
  graph.circles.save({'_key': 'A', 'label': '1'});
  graph.circles.save({'_key': 'B', 'label': '2'});
  graph.circles.save({'_key': 'C', 'label': '3'});
  graph.circles.save({'_key': 'D', 'label': '4'});
  graph.circles.save({'_key': 'E', 'label': '5'});
  graph.circles.save({'_key': 'F', 'label': '6'});
  graph.circles.save({'_key': 'G', 'label': '7'});
  graph.circles.save({'_key': 'H', 'label': '8'});
  graph.circles.save({'_key': 'I', 'label': '9'});
  graph.circles.save({'_key': 'J', 'label': '10'});
  graph.circles.save({'_key': 'K', 'label': '11'});

  // Add relevant edges - left branch:
  graph.edges.save('circles/A', 'circles/B', {theFalse: false, theTruth: true, 'label': 'left_bar'});
  graph.edges.save('circles/B', 'circles/C', {theFalse: false, theTruth: true, 'label': 'left_blarg'});
  graph.edges.save('circles/C', 'circles/D', {theFalse: false, theTruth: true, 'label': 'left_blorg'});
  graph.edges.save('circles/B', 'circles/E', {theFalse: false, theTruth: true, 'label': 'left_blub'});
  graph.edges.save('circles/E', 'circles/F', {theFalse: false, theTruth: true, 'label': 'left_schubi'});

  // Add relevant edges - right branch:
  graph.edges.save('circles/A', 'circles/G', {theFalse: false, theTruth: true, 'label': 'right_foo'});
  graph.edges.save('circles/G', 'circles/H', {theFalse: false, theTruth: true, 'label': 'right_blob'});
  graph.edges.save('circles/H', 'circles/I', {theFalse: false, theTruth: true, 'label': 'right_blub'});
  graph.edges.save('circles/G', 'circles/J', {theFalse: false, theTruth: true, 'label': 'right_zip'});
  graph.edges.save('circles/J', 'circles/K', {theFalse: false, theTruth: true, 'label': 'right_zup'});

  return graph;
};

var createKShortestPathsGraph = function() {
  var graph_module = require('@arangodb/general-graph');

  var graph = graph_module._create('kShortestPathsGraph', [
    graph_module._relation('connections', 'places', 'places')
  ]);

  var places = [
    "Inverness",
    "Aberdeen",
    "Leuchars",
    "StAndrews",
    "Edinburgh",
    "Glasgow",
    "York",
    "Carlisle",
    "Birmingham",
    "London",
    "Brussels",
    "Cologne",
    "Toronto",
    "Winnipeg",
    "Saskatoon",
    "Edmonton",
    "Jasper",
    "Vancouver"
  ];
  var connections = [
    [ "Inverness", "Aberdeen", 3, 2.5 ],
    [ "Aberdeen", "Leuchars", 1.5, 1 ],
    [ "Leuchars", "Edinburgh", 1.5, 3 ],
    [ "Edinburgh", "Glasgow", 1, 1 ],
    [ "Edinburgh", "York", 3.5, 4 ],
    [ "Glasgow", "Carlisle", 1, 1 ],
    [ "Carlisle", "York", 2.5, 3.5 ],
    [ "Carlisle", "Birmingham", 2.0, 1 ],
    [ "Birmingham", "London", 1.5, 2.5 ],
    [ "Leuchars", "StAndrews", 0.2, 0.2 ],
    [ "York", "London", 1.8, 2.0 ],
    [ "London", "Brussels", 2.5, 3.5 ],
    [ "Brussels", "Cologne", 2, 1.5 ],
    [ "Toronto", "Winnipeg", 36, 35 ],
    [ "Winnipeg", "Saskatoon", 12, 5 ],
    [ "Saskatoon", "Edmonton", 12, 17 ],
    [ "Edmonton", "Jasper", 6, 5 ],
    [ "Jasper", "Vancouver", 12, 13 ]
  ];

  for (var p of places) {
    graph.places.save({ _key: p, label: p});
  }
  for (var c of connections) {
    graph.connections.save('places/' + c[0], 'places/' + c[1], {'travelTime': c[2]});
    graph.connections.save('places/' + c[1], 'places/' + c[0], {'travelTime': c[3]});
  }
  return graph;
};

var createConnectedComponentsGraph = function () {

  var graph_module = require('@arangodb/general-graph');

  var graph = graph_module._create('connectedComponentsGraph', [
    graph_module._relation('connections', 'components', 'components')
  ]);

  var edges = [
    ["A1", "A2"],
    ["A2", "A3"],
    ["A3", "A4"],
    ["A4", "A1"],
    ["B1", "B3"],
    ["B2", "B4"],
    ["B3", "B6"],
    ["B4", "B3"],
    ["B4", "B5"],
    ["B6", "B7"],
    ["B7", "B8"],
    ["B7", "B9"],
    ["B7", "B10"],
    ["B7", "B19"],
    ["B11", "B10"],
    ["B12", "B11"],
    ["B13", "B12"],
    ["B13", "B20"],
    ["B14", "B13"],
    ["B15", "B14"],
    ["B15", "B16"],
    ["B17", "B15"],
    ["B17", "B18"],
    ["B19", "B17"],
    ["B20", "B21"],
    ["B20", "B22"],
    ["C1", "C2"],
    ["C2", "C3"],
    ["C3", "C4"],
    ["C4", "C5"],
    ["C4", "C7"],
    ["C5", "C6"],
    ["C5", "C7"],
    ["C7", "C8"],
    ["C8", "C9"],
    ["C8", "C10"],
  ];

  var vertices = new Set(edges.flat());

  for (var vertex of vertices) {
    graph.components.save({ _key: vertex });
  }

  for (var [from, to] of edges) {
    graph.connections.save({ _from: `components/${from}`, _to: `components/${to}` });
  }

  return graph;
};

var knownGraphs = {
  'knows_graph': {create: createTraversalExample, dependencies: [
      'knows', 'persons'
  ]},
  'mps_graph': {create: createMpsTraversal, dependencies: [
      'mps_edges', 'mps_verts'
  ]},
  'routeplanner': {create: createRoutePlannerGraph, dependencies: [
      'frenchHighway', 'frenchCity', 'germanCity', 'germanHighway', 'internationalHighway'
  ]},
  'social': {create: createSocialGraph, dependencies: [
      'relation', 'female', 'male'
  ]},
  'worldCountry': {create: createWorldCountryGraph, dependencies: [
      'worldVertices', 'worldEdges'
  ]},
  'worldCountryUnManaged': {create: createWorldCountryGraphUnManaged, dependencies: [
      'worldVertices', 'worldEdges'
  ]},
  'traversalGraph': {create: createTraversalGraph, dependencies: [
      'edges', 'circles'
  ]},
  'kShortestPathsGraph': {create: createKShortestPathsGraph, dependencies: [
      'connections', 'places'
  ]},
  'connectedComponentsGraph': {create: createConnectedComponentsGraph, dependencies: [
      'connections', 'components'
  ]},
};

var unManagedGraphs = {
  'worldCountryUnManaged': dropWorldCountryUnManaged
};

var dropGraph = function (name, prefixed) {
  if (!knownGraphs.hasOwnProperty(name)) {
    // trying to drop an unknown graph - better not do it
    return false;
  }
  if (unManagedGraphs.hasOwnProperty(name)) {
    unManagedGraphs[name]();
  } else {
    if (prefixed) {
      name = 'UnitTest' + name;
    }
    if (Graph._exists(name)) {
      return Graph._drop(name, true);
    }
  }
};

var loadGraph = function (name, prefixed) {
  if (!knownGraphs.hasOwnProperty(name)) {
    // trying to drop an unknown graph - better not do it
    return false;
  } else {
    var collections = [];

    db._collections().forEach(function (c) {
      collections.push(c.name());
    });

    var dependencies = [];

    knownGraphs[name].dependencies.forEach(function (val) {
      collections.forEach(function (c) {
        if (val === c) {
          dependencies.push(val);
        }
      });
    });

    if (dependencies.length > 0) {
      var err = new ArangoError();
      err.errorNum = arangodb.ERROR_ARANGO_DUPLICATE_NAME;
      if (dependencies.loadGraph > 1) {
        err.errorMessage = 'the collections: ' + JSON.stringify(dependencies) + ' already exist. Please clean up and try again.';
      } else {
        err.errorMessage = 'the collection: ' + JSON.stringify(dependencies) + ' already exists. Please clean up and try again.';
      }
      throw err;
    }
  }

  dropGraph(name);
  return knownGraphs[name].create(prefixed);
};

exports.loadGraph = loadGraph;
exports.dropGraph = dropGraph;

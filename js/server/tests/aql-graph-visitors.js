/*jshint strict: false, sub: true, maxlen: 500 */
/*global require, assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, graph functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("org/arangodb").db;
var graph = require("org/arangodb/general-graph");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for EDGES() function
////////////////////////////////////////////////////////////////////////////////

function ahuacatlGraphVisitorsSuite () {
  var vertex = null;
  var edge   = null;
  var aqlfunctions = require("org/arangodb/aql/functions");

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop("UnitTestsAhuacatlVertex");
      db._drop("UnitTestsAhuacatlEdge");

      vertex = db._create("UnitTestsAhuacatlVertex");
      edge = db._createEdgeCollection("UnitTestsAhuacatlEdge");

      /* vertices: root node */ 
      vertex.save({ _key: "world", name: "World", type: "root" });

      /* vertices: continents */
      vertex.save({ _key: "continent-africa", name: "Africa", type: "continent" });
      vertex.save({ _key: "continent-asia", name: "Asia", type: "continent" });
      vertex.save({ _key: "continent-australia", name: "Australia", type: "continent" });
      vertex.save({ _key: "continent-europe", name: "Europe", type: "continent" });
      vertex.save({ _key: "continent-north-america", name: "North America", type: "continent" });
      vertex.save({ _key: "continent-south-america", name: "South America", type: "continent" });

      /* vertices: countries */
      vertex.save({ _key: "country-afghanistan", name: "Afghanistan", type: "country", code: "AFG" });
      vertex.save({ _key: "country-albania", name: "Albania", type: "country", code: "ALB" });
      vertex.save({ _key: "country-algeria", name: "Algeria", type: "country", code: "DZA" });
      vertex.save({ _key: "country-andorra", name: "Andorra", type: "country", code: "AND" });
      vertex.save({ _key: "country-angola", name: "Angola", type: "country", code: "AGO" });
      vertex.save({ _key: "country-antigua-and-barbuda", name: "Antigua and Barbuda", type: "country", code: "ATG" });
      vertex.save({ _key: "country-argentina", name: "Argentina", type: "country", code: "ARG" });
      vertex.save({ _key: "country-australia", name: "Australia", type: "country", code: "AUS" });
      vertex.save({ _key: "country-austria", name: "Austria", type: "country", code: "AUT" });
      vertex.save({ _key: "country-bahamas", name: "Bahamas", type: "country", code: "BHS" });
      vertex.save({ _key: "country-bahrain", name: "Bahrain", type: "country", code: "BHR" });
      vertex.save({ _key: "country-bangladesh", name: "Bangladesh", type: "country", code: "BGD" });
      vertex.save({ _key: "country-barbados", name: "Barbados", type: "country", code: "BRB" });
      vertex.save({ _key: "country-belgium", name: "Belgium", type: "country", code: "BEL" });
      vertex.save({ _key: "country-bhutan", name: "Bhutan", type: "country", code: "BTN" });
      vertex.save({ _key: "country-bolivia", name: "Bolivia", type: "country", code: "BOL" });
      vertex.save({ _key: "country-bosnia-and-herzegovina", name: "Bosnia and Herzegovina", type: "country", code: "BIH" });
      vertex.save({ _key: "country-botswana", name: "Botswana", type: "country", code: "BWA" });
      vertex.save({ _key: "country-brazil", name: "Brazil", type: "country", code: "BRA" });
      vertex.save({ _key: "country-brunei", name: "Brunei", type: "country", code: "BRN" });
      vertex.save({ _key: "country-bulgaria", name: "Bulgaria", type: "country", code: "BGR" });
      vertex.save({ _key: "country-burkina-faso", name: "Burkina Faso", type: "country", code: "BFA" });
      vertex.save({ _key: "country-burundi", name: "Burundi", type: "country", code: "BDI" });
      vertex.save({ _key: "country-cambodia", name: "Cambodia", type: "country", code: "KHM" });
      vertex.save({ _key: "country-cameroon", name: "Cameroon", type: "country", code: "CMR" });
      vertex.save({ _key: "country-canada", name: "Canada", type: "country", code: "CAN" });
      vertex.save({ _key: "country-chad", name: "Chad", type: "country", code: "TCD" });
      vertex.save({ _key: "country-chile", name: "Chile", type: "country", code: "CHL" });
      vertex.save({ _key: "country-colombia", name: "Colombia", type: "country", code: "COL" });
      vertex.save({ _key: "country-cote-d-ivoire", name: "Cote d'Ivoire", type: "country", code: "CIV" });
      vertex.save({ _key: "country-croatia", name: "Croatia", type: "country", code: "HRV" });
      vertex.save({ _key: "country-czech-republic", name: "Czech Republic", type: "country", code: "CZE" });
      vertex.save({ _key: "country-denmark", name: "Denmark", type: "country", code: "DNK" });
      vertex.save({ _key: "country-ecuador", name: "Ecuador", type: "country", code: "ECU" });
      vertex.save({ _key: "country-egypt", name: "Egypt", type: "country", code: "EGY" });
      vertex.save({ _key: "country-eritrea", name: "Eritrea", type: "country", code: "ERI" });
      vertex.save({ _key: "country-finland", name: "Finland", type: "country", code: "FIN" });
      vertex.save({ _key: "country-france", name: "France", type: "country", code: "FRA" });
      vertex.save({ _key: "country-germany", name: "Germany", type: "country", code: "DEU" });
      vertex.save({ _key: "country-people-s-republic-of-china", name: "People's Republic of China", type: "country", code: "CHN" });

      /* vertices: capitals */ 
      vertex.save({ _key: "capital-algiers", name: "Algiers", type: "capital" });
      vertex.save({ _key: "capital-andorra-la-vella", name: "Andorra la Vella", type: "capital" });
      vertex.save({ _key: "capital-asmara", name: "Asmara", type: "capital" });
      vertex.save({ _key: "capital-bandar-seri-begawan", name: "Bandar Seri Begawan", type: "capital" });
      vertex.save({ _key: "capital-beijing", name: "Beijing", type: "capital" });
      vertex.save({ _key: "capital-berlin", name: "Berlin", type: "capital" });
      vertex.save({ _key: "capital-bogota", name: "Bogota", type: "capital" });
      vertex.save({ _key: "capital-brasilia", name: "Brasilia", type: "capital" });
      vertex.save({ _key: "capital-bridgetown", name: "Bridgetown", type: "capital" });
      vertex.save({ _key: "capital-brussels", name: "Brussels", type: "capital" });
      vertex.save({ _key: "capital-buenos-aires", name: "Buenos Aires", type: "capital" });
      vertex.save({ _key: "capital-bujumbura", name: "Bujumbura", type: "capital" });
      vertex.save({ _key: "capital-cairo", name: "Cairo", type: "capital" });
      vertex.save({ _key: "capital-canberra", name: "Canberra", type: "capital" });
      vertex.save({ _key: "capital-copenhagen", name: "Copenhagen", type: "capital" });
      vertex.save({ _key: "capital-dhaka", name: "Dhaka", type: "capital" });
      vertex.save({ _key: "capital-gaborone", name: "Gaborone", type: "capital" });
      vertex.save({ _key: "capital-helsinki", name: "Helsinki", type: "capital" });
      vertex.save({ _key: "capital-kabul", name: "Kabul", type: "capital" });
      vertex.save({ _key: "capital-la-paz", name: "La Paz", type: "capital" });
      vertex.save({ _key: "capital-luanda", name: "Luanda", type: "capital" });
      vertex.save({ _key: "capital-manama", name: "Manama", type: "capital" });
      vertex.save({ _key: "capital-nassau", name: "Nassau", type: "capital" });
      vertex.save({ _key: "capital-n-djamena", name: "N'Djamena", type: "capital" });
      vertex.save({ _key: "capital-ottawa", name: "Ottawa", type: "capital" });
      vertex.save({ _key: "capital-ouagadougou", name: "Ouagadougou", type: "capital" });
      vertex.save({ _key: "capital-paris", name: "Paris", type: "capital" });
      vertex.save({ _key: "capital-phnom-penh", name: "Phnom Penh", type: "capital" });
      vertex.save({ _key: "capital-prague", name: "Prague", type: "capital" });
      vertex.save({ _key: "capital-quito", name: "Quito", type: "capital" });
      vertex.save({ _key: "capital-saint-john-s", name: "Saint John's", type: "capital" });
      vertex.save({ _key: "capital-santiago", name: "Santiago", type: "capital" });
      vertex.save({ _key: "capital-sarajevo", name: "Sarajevo", type: "capital" });
      vertex.save({ _key: "capital-sofia", name: "Sofia", type: "capital" });
      vertex.save({ _key: "capital-thimphu", name: "Thimphu", type: "capital" });
      vertex.save({ _key: "capital-tirana", name: "Tirana", type: "capital" });
      vertex.save({ _key: "capital-vienna", name: "Vienna", type: "capital" });
      vertex.save({ _key: "capital-yamoussoukro", name: "Yamoussoukro", type: "capital" });
      vertex.save({ _key: "capital-yaounde", name: "Yaounde", type: "capital" });
      vertex.save({ _key: "capital-zagreb", name: "Zagreb", type: "capital" });

      /* edges: continent -> world */ 
      edge.save("UnitTestsAhuacatlVertex/continent-africa", "UnitTestsAhuacatlVertex/world", { _key: "001", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/continent-asia", "UnitTestsAhuacatlVertex/world", { _key: "002", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/continent-australia", "UnitTestsAhuacatlVertex/world", { _key: "003", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/continent-europe", "UnitTestsAhuacatlVertex/world", { _key: "004", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/continent-north-america", "UnitTestsAhuacatlVertex/world", { _key: "005", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/continent-south-america", "UnitTestsAhuacatlVertex/world", { _key: "006", type: "is-in" });

      /* edges: country -> continent */ 
      edge.save("UnitTestsAhuacatlVertex/country-afghanistan", "UnitTestsAhuacatlVertex/continent-asia", { _key: "100", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-albania", "UnitTestsAhuacatlVertex/continent-europe", { _key: "101", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-algeria", "UnitTestsAhuacatlVertex/continent-africa", { _key: "102", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-andorra", "UnitTestsAhuacatlVertex/continent-europe", { _key: "103", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-angola", "UnitTestsAhuacatlVertex/continent-africa", { _key: "104", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-antigua-and-barbuda", "UnitTestsAhuacatlVertex/continent-north-america", { _key: "105", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-argentina", "UnitTestsAhuacatlVertex/continent-south-america", { _key: "106", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-australia", "UnitTestsAhuacatlVertex/continent-australia", { _key: "107", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-austria", "UnitTestsAhuacatlVertex/continent-europe", { _key: "108", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-bahamas", "UnitTestsAhuacatlVertex/continent-north-america", { _key: "109", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-bahrain", "UnitTestsAhuacatlVertex/continent-asia", { _key: "110", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-bangladesh", "UnitTestsAhuacatlVertex/continent-asia", { _key: "111", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-barbados", "UnitTestsAhuacatlVertex/continent-north-america", { _key: "112", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-belgium", "UnitTestsAhuacatlVertex/continent-europe", { _key: "113", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-bhutan", "UnitTestsAhuacatlVertex/continent-asia", { _key: "114", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-bolivia", "UnitTestsAhuacatlVertex/continent-south-america", { _key: "115", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-bosnia-and-herzegovina", "UnitTestsAhuacatlVertex/continent-europe", { _key: "116", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-botswana", "UnitTestsAhuacatlVertex/continent-africa", { _key: "117", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-brazil", "UnitTestsAhuacatlVertex/continent-south-america", { _key: "118", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-brunei", "UnitTestsAhuacatlVertex/continent-asia", { _key: "119", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-bulgaria", "UnitTestsAhuacatlVertex/continent-europe", { _key: "120", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-burkina-faso", "UnitTestsAhuacatlVertex/continent-africa", { _key: "121", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-burundi", "UnitTestsAhuacatlVertex/continent-africa", { _key: "122", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-cambodia", "UnitTestsAhuacatlVertex/continent-asia", { _key: "123", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-cameroon", "UnitTestsAhuacatlVertex/continent-africa", { _key: "124", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-canada", "UnitTestsAhuacatlVertex/continent-north-america", { _key: "125", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-chad", "UnitTestsAhuacatlVertex/continent-africa", { _key: "126", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-chile", "UnitTestsAhuacatlVertex/continent-south-america", { _key: "127", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-colombia", "UnitTestsAhuacatlVertex/continent-south-america", { _key: "128", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-cote-d-ivoire", "UnitTestsAhuacatlVertex/continent-africa", { _key: "129", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-croatia", "UnitTestsAhuacatlVertex/continent-europe", { _key: "130", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-czech-republic", "UnitTestsAhuacatlVertex/continent-europe", { _key: "131", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-denmark", "UnitTestsAhuacatlVertex/continent-europe", { _key: "132", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-ecuador", "UnitTestsAhuacatlVertex/continent-south-america", { _key: "133", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-egypt", "UnitTestsAhuacatlVertex/continent-africa", { _key: "134", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-eritrea", "UnitTestsAhuacatlVertex/continent-africa", { _key: "135", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-finland", "UnitTestsAhuacatlVertex/continent-europe", { _key: "136", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-france", "UnitTestsAhuacatlVertex/continent-europe", { _key: "137", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-germany", "UnitTestsAhuacatlVertex/continent-europe", { _key: "138", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/country-people-s-republic-of-china", "UnitTestsAhuacatlVertex/continent-asia", { _key: "139", type: "is-in" });

      /* edges: capital -> country */ 
      edge.save("UnitTestsAhuacatlVertex/capital-algiers", "UnitTestsAhuacatlVertex/country-algeria", { _key: "200", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-andorra-la-vella", "UnitTestsAhuacatlVertex/country-andorra", { _key: "201", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-asmara", "UnitTestsAhuacatlVertex/country-eritrea", { _key: "202", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-bandar-seri-begawan", "UnitTestsAhuacatlVertex/country-brunei", { _key: "203", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-beijing", "UnitTestsAhuacatlVertex/country-people-s-republic-of-china", { _key: "204", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-berlin", "UnitTestsAhuacatlVertex/country-germany", { _key: "205", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-bogota", "UnitTestsAhuacatlVertex/country-colombia", { _key: "206", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-brasilia", "UnitTestsAhuacatlVertex/country-brazil", { _key: "207", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-bridgetown", "UnitTestsAhuacatlVertex/country-barbados", { _key: "208", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-brussels", "UnitTestsAhuacatlVertex/country-belgium", { _key: "209", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-buenos-aires", "UnitTestsAhuacatlVertex/country-argentina", { _key: "210", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-bujumbura", "UnitTestsAhuacatlVertex/country-burundi", { _key: "211", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-cairo", "UnitTestsAhuacatlVertex/country-egypt", { _key: "212", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-canberra", "UnitTestsAhuacatlVertex/country-australia", { _key: "213", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-copenhagen", "UnitTestsAhuacatlVertex/country-denmark", { _key: "214", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-dhaka", "UnitTestsAhuacatlVertex/country-bangladesh", { _key: "215", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-gaborone", "UnitTestsAhuacatlVertex/country-botswana", { _key: "216", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-helsinki", "UnitTestsAhuacatlVertex/country-finland", { _key: "217", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-kabul", "UnitTestsAhuacatlVertex/country-afghanistan", { _key: "218", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-la-paz", "UnitTestsAhuacatlVertex/country-bolivia", { _key: "219", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-luanda", "UnitTestsAhuacatlVertex/country-angola", { _key: "220", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-manama", "UnitTestsAhuacatlVertex/country-bahrain", { _key: "221", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-nassau", "UnitTestsAhuacatlVertex/country-bahamas", { _key: "222", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-n-djamena", "UnitTestsAhuacatlVertex/country-chad", { _key: "223", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-ottawa", "UnitTestsAhuacatlVertex/country-canada", { _key: "224", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-ouagadougou", "UnitTestsAhuacatlVertex/country-burkina-faso", { _key: "225", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-paris", "UnitTestsAhuacatlVertex/country-france", { _key: "226", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-phnom-penh", "UnitTestsAhuacatlVertex/country-cambodia", { _key: "227", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-prague", "UnitTestsAhuacatlVertex/country-czech-republic", { _key: "228", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-quito", "UnitTestsAhuacatlVertex/country-ecuador", { _key: "229", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-saint-john-s", "UnitTestsAhuacatlVertex/country-antigua-and-barbuda", { _key: "230", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-santiago", "UnitTestsAhuacatlVertex/country-chile", { _key: "231", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-sarajevo", "UnitTestsAhuacatlVertex/country-bosnia-and-herzegovina", { _key: "232", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-sofia", "UnitTestsAhuacatlVertex/country-bulgaria", { _key: "233", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-thimphu", "UnitTestsAhuacatlVertex/country-bhutan", { _key: "234", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-tirana", "UnitTestsAhuacatlVertex/country-albania", { _key: "235", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-vienna", "UnitTestsAhuacatlVertex/country-austria", { _key: "236", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-yamoussoukro", "UnitTestsAhuacatlVertex/country-cote-d-ivoire", { _key: "237", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-yaounde", "UnitTestsAhuacatlVertex/country-cameroon", { _key: "238", type: "is-in" });
      edge.save("UnitTestsAhuacatlVertex/capital-zagreb", "UnitTestsAhuacatlVertex/country-croatia", { _key: "239", type: "is-in" });

      try {
        db._collection("_graphs").remove("_graphs/UnitTestsVisitor");
      } 
      catch (err) {
      }

      graph._create("UnitTestsVisitor", graph._edgeDefinitions(graph._relation(edge.name(), vertex.name(), vertex.name())));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        db._collection("_graphs").remove("_graphs/UnitTestsVisitor");
      } 
      catch (err1) {
      }

      db._drop("UnitTestsAhuacatlVertex");
      db._drop("UnitTestsAhuacatlEdge");

      try {
        aqlfunctions.unregister("UnitTests::visitor");
      }
      catch (err2) {
      }
      
      try {
        aqlfunctions.unregister("UnitTests::filter");
      }
      catch (err3) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testStringifyVisitor : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex, path) {
        var indentation = new Array(path.vertices.length).join("  ");
        var label       = "- " + vertex.name + " (" + vertex.type + ")";
        return indentation + label;
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, visitor : "UnitTests::visitor", visitorReturnsResults : true } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        "- World (root)", 
        "  - Africa (continent)", 
        "    - Algeria (country)", 
        "      - Algiers (capital)", 
        "    - Angola (country)", 
        "      - Luanda (capital)", 
        "    - Botswana (country)", 
        "      - Gaborone (capital)", 
        "    - Burkina Faso (country)", 
        "      - Ouagadougou (capital)", 
        "    - Burundi (country)", 
        "      - Bujumbura (capital)", 
        "    - Cameroon (country)", 
        "      - Yaounde (capital)", 
        "    - Chad (country)", 
        "      - N'Djamena (capital)", 
        "    - Cote d'Ivoire (country)", 
        "      - Yamoussoukro (capital)", 
        "    - Egypt (country)", 
        "      - Cairo (capital)", 
        "    - Eritrea (country)", 
        "      - Asmara (capital)", 
        "  - Asia (continent)", 
        "    - Afghanistan (country)", 
        "      - Kabul (capital)", 
        "    - Bahrain (country)", 
        "      - Manama (capital)", 
        "    - Bangladesh (country)", 
        "      - Dhaka (capital)", 
        "    - Bhutan (country)", 
        "      - Thimphu (capital)", 
        "    - Brunei (country)", 
        "      - Bandar Seri Begawan (capital)", 
        "    - Cambodia (country)", 
        "      - Phnom Penh (capital)", 
        "    - People's Republic of China (country)", 
        "      - Beijing (capital)", 
        "  - Australia (continent)", 
        "    - Australia (country)", 
        "      - Canberra (capital)", 
        "  - Europe (continent)", 
        "    - Albania (country)", 
        "      - Tirana (capital)", 
        "    - Andorra (country)", 
        "      - Andorra la Vella (capital)", 
        "    - Austria (country)", 
        "      - Vienna (capital)", 
        "    - Belgium (country)", 
        "      - Brussels (capital)", 
        "    - Bosnia and Herzegovina (country)", 
        "      - Sarajevo (capital)", 
        "    - Bulgaria (country)", 
        "      - Sofia (capital)", 
        "    - Croatia (country)", 
        "      - Zagreb (capital)", 
        "    - Czech Republic (country)", 
        "      - Prague (capital)", 
        "    - Denmark (country)", 
        "      - Copenhagen (capital)", 
        "    - Finland (country)", 
        "      - Helsinki (capital)", 
        "    - France (country)", 
        "      - Paris (capital)", 
        "    - Germany (country)", 
        "      - Berlin (capital)", 
        "  - North America (continent)", 
        "    - Antigua and Barbuda (country)", 
        "      - Saint John's (capital)", 
        "    - Bahamas (country)", 
        "      - Nassau (capital)", 
        "    - Barbados (country)", 
        "      - Bridgetown (capital)", 
        "    - Canada (country)", 
        "      - Ottawa (capital)", 
        "  - South America (continent)", 
        "    - Argentina (country)", 
        "      - Buenos Aires (capital)", 
        "    - Bolivia (country)", 
        "      - La Paz (capital)", 
        "    - Brazil (country)", 
        "      - Brasilia (capital)", 
        "    - Chile (country)", 
        "      - Santiago (capital)", 
        "    - Colombia (country)", 
        "      - Bogota (capital)", 
        "    - Ecuador (country)", 
        "      - Quito (capital)" 
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testStringifyVisitorTraversalTree : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex, path) {
        if (result.length === 0) {
          result.push({ });
        }

        var current = result[0], connector = config.connect, i;

        for (i = 0; i < path.vertices.length; ++i) {
          var v = path.vertices[i];
          if (typeof current[connector] === "undefined") {
            current[connector] = [ ];
          }
          var found = false, j;
          for (j = 0; j < current[connector].length; ++j) {
            if (current[connector][j]._id === v._id) {
              current = current[connector][j];
              found = true;
              break;
            }
          }
          if (! found) {
            current[connector].push({ name: v.name, _id : v._id });
            current = current[connector][current[connector].length - 1];
          }
        }
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, visitor : "UnitTests::visitor", visitorReturnsResults : false } FOR result IN TRAVERSAL_TREE(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", "children", params) RETURN result');

      var expected = [ 
        [ 
          { 
            "name" : "World", 
            "_id" : "UnitTestsAhuacatlVertex/world", 
            "children" : [ 
              { 
                "name" : "Africa", 
                "_id" : "UnitTestsAhuacatlVertex/continent-africa", 
                "children" : [ 
                  { 
                    "name" : "Algeria", 
                    "_id" : "UnitTestsAhuacatlVertex/country-algeria", 
                    "children" : [ 
                      { 
                        "name" : "Algiers", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-algiers" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Angola", 
                    "_id" : "UnitTestsAhuacatlVertex/country-angola", 
                    "children" : [ 
                      { 
                        "name" : "Luanda", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-luanda" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Botswana", 
                    "_id" : "UnitTestsAhuacatlVertex/country-botswana", 
                    "children" : [ 
                      { 
                        "name" : "Gaborone", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-gaborone" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Burkina Faso", 
                    "_id" : "UnitTestsAhuacatlVertex/country-burkina-faso", 
                    "children" : [ 
                      { 
                        "name" : "Ouagadougou", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-ouagadougou" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Burundi", 
                    "_id" : "UnitTestsAhuacatlVertex/country-burundi", 
                    "children" : [ 
                      { 
                        "name" : "Bujumbura", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-bujumbura" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Cameroon", 
                    "_id" : "UnitTestsAhuacatlVertex/country-cameroon", 
                    "children" : [ 
                      { 
                        "name" : "Yaounde", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-yaounde" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Chad", 
                    "_id" : "UnitTestsAhuacatlVertex/country-chad", 
                    "children" : [ 
                      { 
                        "name" : "N'Djamena", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-n-djamena" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Cote d'Ivoire", 
                    "_id" : "UnitTestsAhuacatlVertex/country-cote-d-ivoire", 
                    "children" : [ 
                      { 
                        "name" : "Yamoussoukro", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-yamoussoukro" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Egypt", 
                    "_id" : "UnitTestsAhuacatlVertex/country-egypt", 
                    "children" : [ 
                      { 
                        "name" : "Cairo", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-cairo" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Eritrea", 
                    "_id" : "UnitTestsAhuacatlVertex/country-eritrea", 
                    "children" : [ 
                      { 
                        "name" : "Asmara", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-asmara" 
                      } 
                    ] 
                  } 
                ] 
              }, 
              { 
                "name" : "Asia", 
                "_id" : "UnitTestsAhuacatlVertex/continent-asia", 
                "children" : [ 
                  { 
                    "name" : "Afghanistan", 
                    "_id" : "UnitTestsAhuacatlVertex/country-afghanistan", 
                    "children" : [ 
                      { 
                        "name" : "Kabul", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-kabul" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Bahrain", 
                    "_id" : "UnitTestsAhuacatlVertex/country-bahrain", 
                    "children" : [ 
                      { 
                        "name" : "Manama", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-manama" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Bangladesh", 
                    "_id" : "UnitTestsAhuacatlVertex/country-bangladesh", 
                    "children" : [ 
                      { 
                        "name" : "Dhaka", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-dhaka" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Bhutan", 
                    "_id" : "UnitTestsAhuacatlVertex/country-bhutan", 
                    "children" : [ 
                      { 
                        "name" : "Thimphu", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-thimphu" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Brunei", 
                    "_id" : "UnitTestsAhuacatlVertex/country-brunei", 
                    "children" : [ 
                      { 
                        "name" : "Bandar Seri Begawan", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-bandar-seri-begawan" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Cambodia", 
                    "_id" : "UnitTestsAhuacatlVertex/country-cambodia", 
                    "children" : [ 
                      { 
                        "name" : "Phnom Penh", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-phnom-penh" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "People's Republic of China", 
                    "_id" : "UnitTestsAhuacatlVertex/country-people-s-republic-of-china", 
                    "children" : [ 
                      { 
                        "name" : "Beijing", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-beijing" 
                      } 
                    ] 
                  } 
                ] 
              }, 
              { 
                "name" : "Australia", 
                "_id" : "UnitTestsAhuacatlVertex/continent-australia", 
                "children" : [ 
                  { 
                    "name" : "Australia", 
                    "_id" : "UnitTestsAhuacatlVertex/country-australia", 
                    "children" : [ 
                      { 
                        "name" : "Canberra", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-canberra" 
                      } 
                    ] 
                  } 
                ] 
              }, 
              { 
                "name" : "Europe", 
                "_id" : "UnitTestsAhuacatlVertex/continent-europe", 
                "children" : [ 
                  { 
                    "name" : "Albania", 
                    "_id" : "UnitTestsAhuacatlVertex/country-albania", 
                    "children" : [ 
                      { 
                        "name" : "Tirana", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-tirana" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Andorra", 
                    "_id" : "UnitTestsAhuacatlVertex/country-andorra", 
                    "children" : [ 
                      { 
                        "name" : "Andorra la Vella", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-andorra-la-vella" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Austria", 
                    "_id" : "UnitTestsAhuacatlVertex/country-austria", 
                    "children" : [ 
                      { 
                        "name" : "Vienna", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-vienna" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Belgium", 
                    "_id" : "UnitTestsAhuacatlVertex/country-belgium", 
                    "children" : [ 
                      { 
                        "name" : "Brussels", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-brussels" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Bosnia and Herzegovina", 
                    "_id" : "UnitTestsAhuacatlVertex/country-bosnia-and-herzegovina", 
                    "children" : [ 
                      { 
                        "name" : "Sarajevo", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-sarajevo" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Bulgaria", 
                    "_id" : "UnitTestsAhuacatlVertex/country-bulgaria", 
                    "children" : [ 
                      { 
                        "name" : "Sofia", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-sofia" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Croatia", 
                    "_id" : "UnitTestsAhuacatlVertex/country-croatia", 
                    "children" : [ 
                      { 
                        "name" : "Zagreb", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-zagreb" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Czech Republic", 
                    "_id" : "UnitTestsAhuacatlVertex/country-czech-republic", 
                    "children" : [ 
                      { 
                        "name" : "Prague", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-prague" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Denmark", 
                    "_id" : "UnitTestsAhuacatlVertex/country-denmark", 
                    "children" : [ 
                      { 
                        "name" : "Copenhagen", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-copenhagen" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Finland", 
                    "_id" : "UnitTestsAhuacatlVertex/country-finland", 
                    "children" : [ 
                      { 
                        "name" : "Helsinki", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-helsinki" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "France", 
                    "_id" : "UnitTestsAhuacatlVertex/country-france", 
                    "children" : [ 
                      { 
                        "name" : "Paris", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-paris" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Germany", 
                    "_id" : "UnitTestsAhuacatlVertex/country-germany", 
                    "children" : [ 
                      { 
                        "name" : "Berlin", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-berlin" 
                      } 
                    ] 
                  } 
                ] 
              }, 
              { 
                "name" : "North America", 
                "_id" : "UnitTestsAhuacatlVertex/continent-north-america", 
                "children" : [ 
                  { 
                    "name" : "Antigua and Barbuda", 
                    "_id" : "UnitTestsAhuacatlVertex/country-antigua-and-barbuda", 
                    "children" : [ 
                      { 
                        "name" : "Saint John's", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-saint-john-s" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Bahamas", 
                    "_id" : "UnitTestsAhuacatlVertex/country-bahamas", 
                    "children" : [ 
                      { 
                        "name" : "Nassau", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-nassau" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Barbados", 
                    "_id" : "UnitTestsAhuacatlVertex/country-barbados", 
                    "children" : [ 
                      { 
                        "name" : "Bridgetown", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-bridgetown" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Canada", 
                    "_id" : "UnitTestsAhuacatlVertex/country-canada", 
                    "children" : [ 
                      { 
                        "name" : "Ottawa", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-ottawa" 
                      } 
                    ] 
                  } 
                ] 
              }, 
              { 
                "name" : "South America", 
                "_id" : "UnitTestsAhuacatlVertex/continent-south-america", 
                "children" : [ 
                  { 
                    "name" : "Argentina", 
                    "_id" : "UnitTestsAhuacatlVertex/country-argentina", 
                    "children" : [ 
                      { 
                        "name" : "Buenos Aires", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-buenos-aires" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Bolivia", 
                    "_id" : "UnitTestsAhuacatlVertex/country-bolivia", 
                    "children" : [ 
                      { 
                        "name" : "La Paz", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-la-paz" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Brazil", 
                    "_id" : "UnitTestsAhuacatlVertex/country-brazil", 
                    "children" : [ 
                      { 
                        "name" : "Brasilia", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-brasilia" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Chile", 
                    "_id" : "UnitTestsAhuacatlVertex/country-chile", 
                    "children" : [ 
                      { 
                        "name" : "Santiago", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-santiago" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Colombia", 
                    "_id" : "UnitTestsAhuacatlVertex/country-colombia", 
                    "children" : [ 
                      { 
                        "name" : "Bogota", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-bogota" 
                      } 
                    ] 
                  }, 
                  { 
                    "name" : "Ecuador", 
                    "_id" : "UnitTestsAhuacatlVertex/country-ecuador", 
                    "children" : [ 
                      { 
                        "name" : "Quito", 
                        "_id" : "UnitTestsAhuacatlVertex/capital-quito" 
                      } 
                    ] 
                  } 
                ] 
              } 
            ] 
          } 
        ] 
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testStructuredVisitor : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex, path) {
        return {
          name: vertex.name,
          type: vertex.type,
          level: path.vertices.length
        };
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, visitor : "UnitTests::visitor", visitorReturnsResults : true } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        { 
          "name" : "World", 
          "type" : "root", 
          "level" : 1 
        }, 
        { 
          "name" : "Africa", 
          "type" : "continent", 
          "level" : 2 
        }, 
        { 
          "name" : "Algeria", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Algiers", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Angola", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Luanda", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Botswana", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Gaborone", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Burkina Faso", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Ouagadougou", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Burundi", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Bujumbura", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Cameroon", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Yaounde", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Chad", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "N'Djamena", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Cote d'Ivoire", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Yamoussoukro", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Egypt", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Cairo", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Eritrea", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Asmara", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Asia", 
          "type" : "continent", 
          "level" : 2 
        }, 
        { 
          "name" : "Afghanistan", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Kabul", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Bahrain", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Manama", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Bangladesh", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Dhaka", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Bhutan", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Thimphu", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Brunei", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Bandar Seri Begawan", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Cambodia", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Phnom Penh", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "People's Republic of China", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Beijing", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Australia", 
          "type" : "continent", 
          "level" : 2 
        }, 
        { 
          "name" : "Australia", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Canberra", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Europe", 
          "type" : "continent", 
          "level" : 2 
        }, 
        { 
          "name" : "Albania", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Tirana", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Andorra", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Andorra la Vella", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Austria", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Vienna", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Belgium", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Brussels", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Bosnia and Herzegovina", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Sarajevo", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Bulgaria", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Sofia", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Croatia", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Zagreb", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Czech Republic", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Prague", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Denmark", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Copenhagen", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Finland", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Helsinki", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "France", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Paris", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Germany", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Berlin", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "North America", 
          "type" : "continent", 
          "level" : 2 
        }, 
        { 
          "name" : "Antigua and Barbuda", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Saint John's", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Bahamas", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Nassau", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Barbados", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Bridgetown", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Canada", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Ottawa", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "South America", 
          "type" : "continent", 
          "level" : 2 
        }, 
        { 
          "name" : "Argentina", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Buenos Aires", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Bolivia", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "La Paz", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Brazil", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Brasilia", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Chile", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Santiago", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Colombia", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Bogota", 
          "type" : "capital", 
          "level" : 4 
        }, 
        { 
          "name" : "Ecuador", 
          "type" : "country", 
          "level" : 3 
        }, 
        { 
          "name" : "Quito", 
          "type" : "capital", 
          "level" : 4 
        } 
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testParentPrinterVisitor : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex, path) {
        var r = {
          name: vertex.name,
          type: vertex.type,
          level: path.vertices.length
        };
        if (path.vertices.length > 1) {
          r.parent = {
            name: path.vertices[path.vertices.length - 2].name,
            type: path.vertices[path.vertices.length - 2].type
          };
        }
        return r;
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, visitor : "UnitTests::visitor", visitorReturnsResults : true } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        { 
          "name" : "World", 
          "type" : "root", 
          "level" : 1 
        }, 
        { 
          "name" : "Africa", 
          "type" : "continent", 
          "level" : 2, 
          "parent" : { 
            "name" : "World", 
            "type" : "root" 
          } 
        }, 
        { 
          "name" : "Algeria", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Algiers", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Algeria", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Angola", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Luanda", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Angola", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Botswana", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Gaborone", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Botswana", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Burkina Faso", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Ouagadougou", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Burkina Faso", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Burundi", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Bujumbura", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Burundi", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Cameroon", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Yaounde", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Cameroon", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Chad", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "N'Djamena", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Chad", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Cote d'Ivoire", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Yamoussoukro", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Cote d'Ivoire", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Egypt", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Cairo", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Egypt", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Eritrea", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Africa", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Asmara", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Eritrea", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Asia", 
          "type" : "continent", 
          "level" : 2, 
          "parent" : { 
            "name" : "World", 
            "type" : "root" 
          } 
        }, 
        { 
          "name" : "Afghanistan", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Asia", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Kabul", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Afghanistan", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Bahrain", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Asia", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Manama", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Bahrain", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Bangladesh", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Asia", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Dhaka", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Bangladesh", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Bhutan", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Asia", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Thimphu", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Bhutan", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Brunei", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Asia", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Bandar Seri Begawan", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Brunei", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Cambodia", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Asia", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Phnom Penh", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Cambodia", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "People's Republic of China", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Asia", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Beijing", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "People's Republic of China", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Australia", 
          "type" : "continent", 
          "level" : 2, 
          "parent" : { 
            "name" : "World", 
            "type" : "root" 
          } 
        }, 
        { 
          "name" : "Australia", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Australia", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Canberra", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Australia", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Europe", 
          "type" : "continent", 
          "level" : 2, 
          "parent" : { 
            "name" : "World", 
            "type" : "root" 
          } 
        }, 
        { 
          "name" : "Albania", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Tirana", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Albania", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Andorra", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Andorra la Vella", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Andorra", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Austria", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Vienna", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Austria", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Belgium", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Brussels", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Belgium", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Bosnia and Herzegovina", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Sarajevo", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Bosnia and Herzegovina", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Bulgaria", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Sofia", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Bulgaria", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Croatia", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Zagreb", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Croatia", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Czech Republic", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Prague", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Czech Republic", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Denmark", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Copenhagen", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Denmark", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Finland", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Helsinki", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Finland", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "France", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Paris", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "France", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Germany", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "Europe", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Berlin", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Germany", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "North America", 
          "type" : "continent", 
          "level" : 2, 
          "parent" : { 
            "name" : "World", 
            "type" : "root" 
          } 
        }, 
        { 
          "name" : "Antigua and Barbuda", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "North America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Saint John's", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Antigua and Barbuda", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Bahamas", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "North America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Nassau", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Bahamas", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Barbados", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "North America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Bridgetown", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Barbados", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Canada", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "North America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Ottawa", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Canada", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "South America", 
          "type" : "continent", 
          "level" : 2, 
          "parent" : { 
            "name" : "World", 
            "type" : "root" 
          } 
        }, 
        { 
          "name" : "Argentina", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "South America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Buenos Aires", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Argentina", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Bolivia", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "South America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "La Paz", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Bolivia", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Brazil", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "South America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Brasilia", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Brazil", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Chile", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "South America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Santiago", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Chile", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Colombia", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "South America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Bogota", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Colombia", 
            "type" : "country" 
          } 
        }, 
        { 
          "name" : "Ecuador", 
          "type" : "country", 
          "level" : 3, 
          "parent" : { 
            "name" : "South America", 
            "type" : "continent" 
          } 
        }, 
        { 
          "name" : "Quito", 
          "type" : "capital", 
          "level" : 4, 
          "parent" : { 
            "name" : "Ecuador", 
            "type" : "country" 
          } 
        } 
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testLeafNodeVisitorWithoutCorrectOrder : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex, path, connected) {
        if (connected && connected.length === 0) {
          return 1;
        }
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, visitor : "UnitTests::visitor", visitorReturnsResults : true } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        // intentionally empty
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testLeafNodeVisitor : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex, path, connected) {
        if (connected && connected.length === 0) {
          return vertex.name + " (" + vertex.type + ")";
        }
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, visitor : "UnitTests::visitor", visitorReturnsResults : true, order : "preorder-expander" } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        "Algiers (capital)", 
        "Luanda (capital)", 
        "Gaborone (capital)", 
        "Ouagadougou (capital)", 
        "Bujumbura (capital)", 
        "Yaounde (capital)", 
        "N'Djamena (capital)", 
        "Yamoussoukro (capital)", 
        "Cairo (capital)", 
        "Asmara (capital)", 
        "Kabul (capital)", 
        "Manama (capital)", 
        "Dhaka (capital)", 
        "Thimphu (capital)", 
        "Bandar Seri Begawan (capital)", 
        "Phnom Penh (capital)", 
        "Beijing (capital)", 
        "Canberra (capital)", 
        "Tirana (capital)", 
        "Andorra la Vella (capital)", 
        "Vienna (capital)", 
        "Brussels (capital)", 
        "Sarajevo (capital)", 
        "Sofia (capital)", 
        "Zagreb (capital)", 
        "Prague (capital)", 
        "Copenhagen (capital)", 
        "Helsinki (capital)", 
        "Paris (capital)", 
        "Berlin (capital)", 
        "Saint John's (capital)", 
        "Nassau (capital)", 
        "Bridgetown (capital)", 
        "Ottawa (capital)", 
        "Buenos Aires (capital)", 
        "La Paz (capital)", 
        "Brasilia (capital)", 
        "Santiago (capital)", 
        "Bogota (capital)", 
        "Quito (capital)" 
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testLeafNodeVisitorGeneralGraph : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex, path, connected) {
        if (connected && connected.length === 0) {
          return vertex.name + " (" + vertex.type + ")";
        }
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, visitor : "UnitTests::visitor", visitorReturnsResults : true, order : "preorder-expander" } FOR result IN GRAPH_TRAVERSAL("UnitTestsVisitor", "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        "Algiers (capital)", 
        "Luanda (capital)", 
        "Gaborone (capital)", 
        "Ouagadougou (capital)", 
        "Bujumbura (capital)", 
        "Yaounde (capital)", 
        "N'Djamena (capital)", 
        "Yamoussoukro (capital)", 
        "Cairo (capital)", 
        "Asmara (capital)", 
        "Kabul (capital)", 
        "Manama (capital)", 
        "Dhaka (capital)", 
        "Thimphu (capital)", 
        "Bandar Seri Begawan (capital)", 
        "Phnom Penh (capital)", 
        "Beijing (capital)", 
        "Canberra (capital)", 
        "Tirana (capital)", 
        "Andorra la Vella (capital)", 
        "Vienna (capital)", 
        "Brussels (capital)", 
        "Sarajevo (capital)", 
        "Sofia (capital)", 
        "Zagreb (capital)", 
        "Prague (capital)", 
        "Copenhagen (capital)", 
        "Helsinki (capital)", 
        "Paris (capital)", 
        "Berlin (capital)", 
        "Saint John's (capital)", 
        "Nassau (capital)", 
        "Bridgetown (capital)", 
        "Ottawa (capital)", 
        "Buenos Aires (capital)", 
        "La Paz (capital)", 
        "Brasilia (capital)", 
        "Santiago (capital)", 
        "Bogota (capital)", 
        "Quito (capital)" 
      ];

      assertEqual([ expected ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testLeafNodeVisitorInPlace : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex, path, connected) {
        if (connected && connected.length === 0) {
          result.push(vertex.name + " (" + vertex.type + ")");
        }
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, visitor : "UnitTests::visitor", visitorReturnsResults : false, order : "preorder-expander" } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        "Algiers (capital)", 
        "Luanda (capital)", 
        "Gaborone (capital)", 
        "Ouagadougou (capital)", 
        "Bujumbura (capital)", 
        "Yaounde (capital)", 
        "N'Djamena (capital)", 
        "Yamoussoukro (capital)", 
        "Cairo (capital)", 
        "Asmara (capital)", 
        "Kabul (capital)", 
        "Manama (capital)", 
        "Dhaka (capital)", 
        "Thimphu (capital)", 
        "Bandar Seri Begawan (capital)", 
        "Phnom Penh (capital)", 
        "Beijing (capital)", 
        "Canberra (capital)", 
        "Tirana (capital)", 
        "Andorra la Vella (capital)", 
        "Vienna (capital)", 
        "Brussels (capital)", 
        "Sarajevo (capital)", 
        "Sofia (capital)", 
        "Zagreb (capital)", 
        "Prague (capital)", 
        "Copenhagen (capital)", 
        "Helsinki (capital)", 
        "Paris (capital)", 
        "Berlin (capital)", 
        "Saint John's (capital)", 
        "Nassau (capital)", 
        "Bridgetown (capital)", 
        "Ottawa (capital)", 
        "Buenos Aires (capital)", 
        "La Paz (capital)", 
        "Brasilia (capital)", 
        "Santiago (capital)", 
        "Bogota (capital)", 
        "Quito (capital)" 
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testLeafNodeVisitorPathAccessor : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex, path, connected) {
        if (connected && connected.length === 0) {
          var res = "";
          path.vertices.forEach(function(v, i) {
            if (i > 0 && i <= path.edges.length) {
              res += " <--[" + path.edges[i - 1].type + "]-- ";
            }
            res += v.name;
          });
          return res;
        }
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, visitor : "UnitTests::visitor", visitorReturnsResults : true, order : "preorder-expander" } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        "World <--[is-in]-- Africa <--[is-in]-- Algeria <--[is-in]-- Algiers", 
        "World <--[is-in]-- Africa <--[is-in]-- Angola <--[is-in]-- Luanda", 
        "World <--[is-in]-- Africa <--[is-in]-- Botswana <--[is-in]-- Gaborone", 
        "World <--[is-in]-- Africa <--[is-in]-- Burkina Faso <--[is-in]-- Ouagadougou", 
        "World <--[is-in]-- Africa <--[is-in]-- Burundi <--[is-in]-- Bujumbura", 
        "World <--[is-in]-- Africa <--[is-in]-- Cameroon <--[is-in]-- Yaounde", 
        "World <--[is-in]-- Africa <--[is-in]-- Chad <--[is-in]-- N'Djamena", 
        "World <--[is-in]-- Africa <--[is-in]-- Cote d'Ivoire <--[is-in]-- Yamoussoukro", 
        "World <--[is-in]-- Africa <--[is-in]-- Egypt <--[is-in]-- Cairo", 
        "World <--[is-in]-- Africa <--[is-in]-- Eritrea <--[is-in]-- Asmara", 
        "World <--[is-in]-- Asia <--[is-in]-- Afghanistan <--[is-in]-- Kabul", 
        "World <--[is-in]-- Asia <--[is-in]-- Bahrain <--[is-in]-- Manama", 
        "World <--[is-in]-- Asia <--[is-in]-- Bangladesh <--[is-in]-- Dhaka", 
        "World <--[is-in]-- Asia <--[is-in]-- Bhutan <--[is-in]-- Thimphu", 
        "World <--[is-in]-- Asia <--[is-in]-- Brunei <--[is-in]-- Bandar Seri Begawan", 
        "World <--[is-in]-- Asia <--[is-in]-- Cambodia <--[is-in]-- Phnom Penh", 
        "World <--[is-in]-- Asia <--[is-in]-- People's Republic of China <--[is-in]-- Beijing", 
        "World <--[is-in]-- Australia <--[is-in]-- Australia <--[is-in]-- Canberra", 
        "World <--[is-in]-- Europe <--[is-in]-- Albania <--[is-in]-- Tirana", 
        "World <--[is-in]-- Europe <--[is-in]-- Andorra <--[is-in]-- Andorra la Vella", 
        "World <--[is-in]-- Europe <--[is-in]-- Austria <--[is-in]-- Vienna", 
        "World <--[is-in]-- Europe <--[is-in]-- Belgium <--[is-in]-- Brussels", 
        "World <--[is-in]-- Europe <--[is-in]-- Bosnia and Herzegovina <--[is-in]-- Sarajevo", 
        "World <--[is-in]-- Europe <--[is-in]-- Bulgaria <--[is-in]-- Sofia", 
        "World <--[is-in]-- Europe <--[is-in]-- Croatia <--[is-in]-- Zagreb", 
        "World <--[is-in]-- Europe <--[is-in]-- Czech Republic <--[is-in]-- Prague", 
        "World <--[is-in]-- Europe <--[is-in]-- Denmark <--[is-in]-- Copenhagen", 
        "World <--[is-in]-- Europe <--[is-in]-- Finland <--[is-in]-- Helsinki", 
        "World <--[is-in]-- Europe <--[is-in]-- France <--[is-in]-- Paris", 
        "World <--[is-in]-- Europe <--[is-in]-- Germany <--[is-in]-- Berlin", 
        "World <--[is-in]-- North America <--[is-in]-- Antigua and Barbuda <--[is-in]-- Saint John's", 
        "World <--[is-in]-- North America <--[is-in]-- Bahamas <--[is-in]-- Nassau", 
        "World <--[is-in]-- North America <--[is-in]-- Barbados <--[is-in]-- Bridgetown", 
        "World <--[is-in]-- North America <--[is-in]-- Canada <--[is-in]-- Ottawa", 
        "World <--[is-in]-- South America <--[is-in]-- Argentina <--[is-in]-- Buenos Aires", 
        "World <--[is-in]-- South America <--[is-in]-- Bolivia <--[is-in]-- La Paz", 
        "World <--[is-in]-- South America <--[is-in]-- Brazil <--[is-in]-- Brasilia", 
        "World <--[is-in]-- South America <--[is-in]-- Chile <--[is-in]-- Santiago", 
        "World <--[is-in]-- South America <--[is-in]-- Colombia <--[is-in]-- Bogota", 
        "World <--[is-in]-- South America <--[is-in]-- Ecuador <--[is-in]-- Quito" 
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testCountingVisitorInPlace : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result) {
        if (result.length === 0) {
          result.push(0);
        }
        result[0]++;
      });

      var result = AQL_EXECUTE('LET params = { visitor : "UnitTests::visitor", visitorReturnsResults : false } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      assertEqual([ 87 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testCountingVisitorInPlaceGeneralGraph : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result) {
        if (result.length === 0) {
          result.push(0);
        }
        result[0]++;
      });

      var result = AQL_EXECUTE('LET params = { visitor : "UnitTests::visitor", visitorReturnsResults : false } FOR result IN GRAPH_TRAVERSAL("UnitTestsVisitor", "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      assertEqual([ [ 87 ] ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testTypeCountingVisitorInPlace : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex) {
        if (result.length === 0) {
          result.push({ });
        }
        var vertexType = vertex.type;
        if (! result[0].hasOwnProperty(vertexType)) {
          result[0][vertexType] = 1;
        }
        else {
          result[0][vertexType]++;
        }
      });

      var result = AQL_EXECUTE('LET params = { visitor : "UnitTests::visitor", visitorReturnsResults : false } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');
        
      var expected = [
        { 
          "root" : 1,
          "continent" : 6,
          "country" : 40,
          "capital" : 40
        }
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom visitor
////////////////////////////////////////////////////////////////////////////////

    testTypeCountingVisitorInPlaceGeneralGraph : function () {
      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex) {
        if (result.length === 0) {
          result.push({ });
        }
        var vertexType = vertex.type;
        if (! result[0].hasOwnProperty(vertexType)) {
          result[0][vertexType] = 1;
        }
        else {
          result[0][vertexType]++;
        }
      });

      var result = AQL_EXECUTE('LET params = { visitor : "UnitTests::visitor", visitorReturnsResults : false } FOR result IN GRAPH_TRAVERSAL("UnitTestsVisitor", "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');
        
      var expected = [
        { 
          "root" : 1,
          "continent" : 6,
          "country" : 40,
          "capital" : 40
        }
      ];

      assertEqual([ expected ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom filter
////////////////////////////////////////////////////////////////////////////////

    testFilter : function () {
      aqlfunctions.register("UnitTests::filter", function (config, vertex) {
        if (vertex.type === "country") {
          return "prune";
        }

        if (vertex.type === "continent") {
          if (vertex.name !== "Europe") {
            return [ "prune", "exclude" ];
          }
        }

        return "exclude";
      });

      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex) {
        return vertex.name;
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, filterVertices : "UnitTests::filter", visitor : "UnitTests::visitor", visitorReturnsResults : true } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        "Albania", 
        "Andorra", 
        "Austria", 
        "Belgium", 
        "Bosnia and Herzegovina", 
        "Bulgaria", 
        "Croatia", 
        "Czech Republic", 
        "Denmark", 
        "Finland", 
        "France", 
        "Germany" 
      ];

      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a custom filter
////////////////////////////////////////////////////////////////////////////////

    testFilterGeneralGraph : function () {
      aqlfunctions.register("UnitTests::filter", function (config, vertex) {
        if (vertex.type === "country") {
          return "prune";
        }

        if (vertex.type === "continent") {
          if (vertex.name !== "Europe") {
            return [ "prune", "exclude" ];
          }
        }

        return "exclude";
      });

      aqlfunctions.register("UnitTests::visitor", function (config, result, vertex) {
        return vertex.name;
      });

      var result = AQL_EXECUTE('LET params = { _sort : true, filterVertices : "UnitTests::filter", visitor : "UnitTests::visitor", visitorReturnsResults : true } FOR result IN GRAPH_TRAVERSAL("UnitTestsVisitor", "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [
        "Albania", 
        "Andorra", 
        "Austria", 
        "Belgium", 
        "Bosnia and Herzegovina", 
        "Bulgaria", 
        "Croatia", 
        "Czech Republic", 
        "Denmark", 
        "Finland", 
        "France", 
        "Germany" 
      ];

      assertEqual([ expected ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests a built-in visitor
////////////////////////////////////////////////////////////////////////////////

    testBuiltinCountingVisitor : function () {
      var result = AQL_EXECUTE('LET params = { visitor : "_AQL::COUNTINGVISITOR" } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');
        
      assertEqual([ 87 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests a built-in visitor
////////////////////////////////////////////////////////////////////////////////

    testBuiltinHasAnyAttributesVisitor : function () {
      vertex.save({ _key: "test", foo: "bar" }); 
      vertex.save({ _key: "foo", bar: "baz", foo: "bar" });
      edge.save("UnitTestsAhuacatlVertex/test", "UnitTestsAhuacatlVertex/foo", { });
      var result = AQL_EXECUTE('LET params = { visitor : "_AQL::HASATTRIBUTESVISITOR", data: { attributes: [ "foo", "bar" ], type: "any" } } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/foo", "inbound", params) RETURN result.vertex._key');

      assertEqual([ "foo", "test" ], result.json.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests a built-in visitor
////////////////////////////////////////////////////////////////////////////////

    testBuiltinHasAllAttributesVisitor : function () {
      vertex.save({ _key: "test", foo: "bar" }); 
      vertex.save({ _key: "foo", bar: "baz", foo: "bar" }); 
      edge.save("UnitTestsAhuacatlVertex/test", "UnitTestsAhuacatlVertex/foo", { });
      var result = AQL_EXECUTE('LET params = { visitor : "_AQL::HASATTRIBUTESVISITOR", data: { attributes: [ "foo", "bar" ], type: "all" } } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/foo", "inbound", params) RETURN result.vertex._key');

      assertEqual([ "foo" ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests a built-in visitor
////////////////////////////////////////////////////////////////////////////////

    testBuiltinProjectingVisitor : function () {
      var result = AQL_EXECUTE('LET params = { _sort: true, visitor : "_AQL::PROJECTINGVISITOR", data : { attributes: [ "name", "type" ] } } FOR result IN TRAVERSAL(UnitTestsAhuacatlVertex, UnitTestsAhuacatlEdge, "UnitTestsAhuacatlVertex/world", "inbound", params) RETURN result');

      var expected = [  
        { 
          "name" : "World", 
          "type" : "root" 
        }, 
        { 
          "name" : "Africa", 
          "type" : "continent" 
        }, 
        { 
          "name" : "Algeria", 
          "type" : "country" 
        }, 
        { 
          "name" : "Algiers", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Angola", 
          "type" : "country" 
        }, 
        { 
          "name" : "Luanda", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Botswana", 
          "type" : "country" 
        }, 
        { 
          "name" : "Gaborone", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Burkina Faso", 
          "type" : "country" 
        }, 
        { 
          "name" : "Ouagadougou", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Burundi", 
          "type" : "country" 
        }, 
        { 
          "name" : "Bujumbura", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Cameroon", 
          "type" : "country" 
        }, 
        { 
          "name" : "Yaounde", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Chad", 
          "type" : "country" 
        }, 
        { 
          "name" : "N'Djamena", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Cote d'Ivoire", 
          "type" : "country" 
        }, 
        { 
          "name" : "Yamoussoukro", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Egypt", 
          "type" : "country" 
        }, 
        { 
          "name" : "Cairo", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Eritrea", 
          "type" : "country" 
        }, 
        { 
          "name" : "Asmara", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Asia", 
          "type" : "continent" 
        }, 
        { 
          "name" : "Afghanistan", 
          "type" : "country" 
        }, 
        { 
          "name" : "Kabul", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Bahrain", 
          "type" : "country" 
        }, 
        { 
          "name" : "Manama", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Bangladesh", 
          "type" : "country" 
        }, 
        { 
          "name" : "Dhaka", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Bhutan", 
          "type" : "country" 
        }, 
        { 
          "name" : "Thimphu", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Brunei", 
          "type" : "country" 
        }, 
        { 
          "name" : "Bandar Seri Begawan", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Cambodia", 
          "type" : "country" 
        }, 
        { 
          "name" : "Phnom Penh", 
          "type" : "capital" 
        }, 
        { 
          "name" : "People's Republic of China", 
          "type" : "country" 
        }, 
        { 
          "name" : "Beijing", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Australia", 
          "type" : "continent" 
        }, 
        { 
          "name" : "Australia", 
          "type" : "country" 
        }, 
        { 
          "name" : "Canberra", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Europe", 
          "type" : "continent" 
        }, 
        { 
          "name" : "Albania", 
          "type" : "country" 
        }, 
        { 
          "name" : "Tirana", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Andorra", 
          "type" : "country" 
        }, 
        { 
          "name" : "Andorra la Vella", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Austria", 
          "type" : "country" 
        }, 
        { 
          "name" : "Vienna", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Belgium", 
          "type" : "country" 
        }, 
        { 
          "name" : "Brussels", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Bosnia and Herzegovina", 
          "type" : "country" 
        }, 
        { 
          "name" : "Sarajevo", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Bulgaria", 
          "type" : "country" 
        }, 
        { 
          "name" : "Sofia", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Croatia", 
          "type" : "country" 
        }, 
        { 
          "name" : "Zagreb", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Czech Republic", 
          "type" : "country" 
        }, 
        { 
          "name" : "Prague", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Denmark", 
          "type" : "country" 
        }, 
        { 
          "name" : "Copenhagen", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Finland", 
          "type" : "country" 
        }, 
        { 
          "name" : "Helsinki", 
          "type" : "capital" 
        }, 
        { 
          "name" : "France", 
          "type" : "country" 
        }, 
        { 
          "name" : "Paris", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Germany", 
          "type" : "country" 
        }, 
        { 
          "name" : "Berlin", 
          "type" : "capital" 
        }, 
        { 
          "name" : "North America", 
          "type" : "continent" 
        }, 
        { 
          "name" : "Antigua and Barbuda", 
          "type" : "country" 
        }, 
        { 
          "name" : "Saint John's", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Bahamas", 
          "type" : "country" 
        }, 
        { 
          "name" : "Nassau", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Barbados", 
          "type" : "country" 
        }, 
        { 
          "name" : "Bridgetown", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Canada", 
          "type" : "country" 
        }, 
        { 
          "name" : "Ottawa", 
          "type" : "capital" 
        }, 
        { 
          "name" : "South America", 
          "type" : "continent" 
        }, 
        { 
          "name" : "Argentina", 
          "type" : "country" 
        }, 
        { 
          "name" : "Buenos Aires", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Bolivia", 
          "type" : "country" 
        }, 
        { 
          "name" : "La Paz", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Brazil", 
          "type" : "country" 
        }, 
        { 
          "name" : "Brasilia", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Chile", 
          "type" : "country" 
        }, 
        { 
          "name" : "Santiago", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Colombia", 
          "type" : "country" 
        }, 
        { 
          "name" : "Bogota", 
          "type" : "capital" 
        }, 
        { 
          "name" : "Ecuador", 
          "type" : "country" 
        }, 
        { 
          "name" : "Quito", 
          "type" : "capital" 
        } 
      ];
        
      assertEqual(expected, result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests a built-in visitor
////////////////////////////////////////////////////////////////////////////////

    testNeighborsBuiltinHasAnyAttributesVisitor : function () {
      var graphModule = require("org/arangodb/general-graph");

      try {
        graphModule._drop("UnitTestsAhuacatlGraph");
      }
      catch (err) {
      }
      var graph = graphModule._create("UnitTestsAhuacatlGraph");
      graph._addVertexCollection(vertex.name());
      var rel = graphModule._relation(edge.name(), [ vertex.name() ], [ vertex.name() ]);
      graph._extendEdgeDefinitions(rel);

      vertex.save({ _key: "newRoot", foo: "bar" }); 
      vertex.save({ _key: "test", foo: "bar" }); 
      vertex.save({ _key: "foo", bar: "baz", foo: "bar" });
      edge.save("UnitTestsAhuacatlVertex/newRoot", "UnitTestsAhuacatlVertex/test", { });
      edge.save("UnitTestsAhuacatlVertex/newRoot", "UnitTestsAhuacatlVertex/foo", { });
      var result = AQL_EXECUTE('LET params = { direction: "any", visitor : "_AQL::HASATTRIBUTESVISITOR", data: { attributes: [ "foo", "bar" ], type: "any" } } FOR result IN GRAPH_NEIGHBORS("UnitTestsAhuacatlGraph", "UnitTestsAhuacatlVertex/newRoot", params) RETURN result.vertex._key');

      assertEqual([ "foo", "test" ], result.json.sort());
      try {
        graphModule._drop("UnitTestsAhuacatlGraph");
      }
      catch (err) {
      }
    }

  };

}  

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlGraphVisitorsSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

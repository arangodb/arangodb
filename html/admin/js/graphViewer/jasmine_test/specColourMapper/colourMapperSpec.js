/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global NodeShaper*/

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
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
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function () {
  "use strict";
 
  describe('Colour Mapper', function() {
    
    var svg, mapper;
    
    beforeEach(function () {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
      mapper = new ColourMapper(d3.select("svg"));
    });

    afterEach(function () {
      document.body.removeChild(svg);
    });
    
    it('should automatically add the list of colours to svg defs', function() {
      mapper.getColour("42");
      expect($("svg defs solidColor").length).toEqual(20);
    });
    
    it('should not add the mapping table twice', function() {
      mapper.getColour("42");
      var mapper2 = new ColourMapper(d3.select("svg"));
      mapper2.getColour("23");
      expect($("svg defs solidColor").length).toEqual(20);
    });
    
    it('should return a reference to a colour', function() {
      var colourRef = mapper.getColour("42");
      
      expect(colourRef).toBeDefined();
      expect($(colourRef).length).toEqual(1);
    });
    
    it('should return a different reference for different values', function() {
      var c1 = mapper.getColour("42"),
      c2 = mapper.getColour("23");
      
      expect(c1).not.toEqual(c2);
    });
    
    it('should return the same reference for equal values', function() {
      var c1 = mapper.getColour("42"),
      c2 = mapper.getColour("42");
      
      expect(c1).toEqual(c2);
    });
    
    it('should return references for non string values', function() {
      var c1 = mapper.getColour(42),
      c2 = mapper.getColour(true);
      
      expect(c1).toBeDefined();
      expect($(c1).length).toEqual(1);
      
      expect(c2).toBeDefined();
      expect($(c2).length).toEqual(1);
      
      expect(c1).not.toEqual(c2);
    });
    
    
    it('should return 20 different colours and than restart', function() {
      var colours = [],
      i, j;
      colours.push(mapper.getColour("1"));
      colours.push(mapper.getColour("2"));
      colours.push(mapper.getColour("3"));
      colours.push(mapper.getColour("4"));
      colours.push(mapper.getColour("5"));
      colours.push(mapper.getColour("6"));
      colours.push(mapper.getColour("7"));
      colours.push(mapper.getColour("8"));
      colours.push(mapper.getColour("9"));
      colours.push(mapper.getColour("10"));
      colours.push(mapper.getColour("11"));
      colours.push(mapper.getColour("12"));
      colours.push(mapper.getColour("13"));
      colours.push(mapper.getColour("14"));
      colours.push(mapper.getColour("15"));
      colours.push(mapper.getColour("16"));
      colours.push(mapper.getColour("17"));
      colours.push(mapper.getColour("18"));
      colours.push(mapper.getColour("19"));
      colours.push(mapper.getColour("20"));
      
      for (i = 0; i < colours.length; i++) {
        for (j = i; j < colours.length; j++) {
          expect(colours[i]).not.toEqual(colours[j]);
        }
      }
    });
    
  });
  
}());
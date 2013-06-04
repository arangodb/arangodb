/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine */
/*global document */
/*global $, d3, _*/
/*global ColourMapper*/

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
    
    var mapper;
    
    beforeEach(function () {
      mapper = new ColourMapper();
    });
    
    it('should return a colour', function() {
      var colourRef = mapper.getColour("42");
      
      expect(colourRef).toBeDefined();
    });
    
    it('should return a different colour for different values', function() {
      var c1 = mapper.getColour("42"),
      c2 = mapper.getColour("23");
      
      expect(c1).not.toEqual(c2);
    });
    
    it('should return the same colour for equal values', function() {
      var c1 = mapper.getColour("42"),
      c2 = mapper.getColour("42");
      
      expect(c1).toEqual(c2);
    });
    
    it('should return colours for non string values', function() {
      var c1 = mapper.getColour(42),
      c2 = mapper.getColour(true);
      
      expect(c1).toBeDefined();      
      expect(c2).toBeDefined();
      expect(c1).not.toEqual(c2);
    });
    
    it('should be able to manually reset the returned colours', function() {
      var c1 = mapper.getColour("1"),
      c2 = mapper.getColour("2"),
      c3;
      
      mapper.reset();
      
      c3 = mapper.getColour("3");
      
      expect(c1).toEqual(c3);
    });
    
    it('should return 20 different colours and than restart', function() {
      var colours = [],
      i, j, cNew;
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
      cNew = mapper.getColour("21");
      for (i = 0; i < colours.length; i++) {
        for (j = i + 1; j < colours.length; j++) {
          expect(colours[i]).not.toEqual(colours[j]);
        }
      }
      expect(colours[0]).toEqual(cNew);
    });
    
    it('should be able to return the colour mapping list', function() {
      mapper.getColour("1");
      mapper.getColour("2");
      mapper.getColour("3");
      
      var colorList = mapper.getList();
      
      expect(_.keys(colorList).length).toEqual(3);
      _.each(_.values(colorList), function(v) {
        expect(v).toEqual(jasmine.any(Array));
        expect(v.length).toEqual(1);
      });
      
    });
    
    it('should be possible to add a change listener', function() {
      var res = {},
        correct = {},
        listener = function(mapping) {
          res = mapping;
        };
        
      mapper.setChangeListener(listener);
      mapper.getColour("1");
      correct = mapper.getList();
      
      expect(res).toEqual(correct);
    });
  });
  
}());
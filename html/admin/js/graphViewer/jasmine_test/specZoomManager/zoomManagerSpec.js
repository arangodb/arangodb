/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global ZoomManager*/

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

  describe('Zoom Manager', function () {
    var svg;

    beforeEach(function () {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
    });

    afterEach(function () {
      document.body.removeChild(svg);
    });

    describe('setup process', function() {
      
      it('should throw an error if no width is given', function() {
        expect(function() {
          var s = new ZoomManager();
        }).toThrow("A width has to be given.");
      });
      
      it('should throw an error if no height is given', function() {
        expect(function() {
          var s = new ZoomManager(10);
        }).toThrow("A height has to be given.");
      });
      
      it('should not throw an error if mandatory information is given', function() {
        expect(function() {
          var s = new ZoomManager(10, 10);
        }).not.toThrow();
      });
    });

  });

  describe('setup correctly', function() {
    
    var manager;
    
    beforeEach(function() {
      manager = new ZoomManager(10, 10);
    });
    
    it('should offer a function to get the current font-size', function() {
      expect(manager.getFontSize).toBeDefined();
      expect(manager.getFontSize).toEqual(Jasmine.any(Function));
    });
    
    it('should offer a function to get the current node-radius', function() {
      expect(manager.getRadius).toBeDefined();
      expect(manager.getRadius).toEqual(Jasmine.any(Function));
    });
    
    it('should offer a function for zoom-in', function() {
      expect(manager.zoomIn).toBeDefined();
      expect(manager.zoomIn).toEqual(Jasmine.any(Function));
    });
    
    it('should offer a function for zoom-out', function() {
      expect(manager.zoomOut).toBeDefined();
      expect(manager.zoomOut).toEqual(Jasmine.any(Function));
    });
    
    
    
    
  });

}());
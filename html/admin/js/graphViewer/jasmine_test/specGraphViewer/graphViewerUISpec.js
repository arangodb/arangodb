/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine*/
/*global runs, waitsFor, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper, mocks*/
/*global GraphViewerUI*/

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

  describe('Graph Viewer UI', function () {
    
    var div,
    ui;
    
    beforeEach(function() {
      div = document.createElement("div");
      div.id = "contentDiv";
      document.body.appendChild(div);
      ui = new GraphViewerUI(div);
    });
    
    afterEach(function() {
      document.removeChild(div);
    });
    
    it('should append a svg to the given parent', function() {
      expect($("#contentDiv svg").length).toEqual(1);
    });
    
    describe('checking the toolbox', function() {
      var toolboxSelector = "#contentDiv #toolbox";
      
      it('should append the toolbox', function() {
        expect($(toolboxSelector).length).toEqual(1);
      });
      
      it('should contain the objects from eventDispatcher', function() {
        
        expect($(toolboxSelector + " #control_drag").length).toEqual(1);
        expect($(toolboxSelector + " #control_edit").length).toEqual(1);
        expect($(toolboxSelector + " #control_expand").length).toEqual(1);
        expect($(toolboxSelector + " #control_delete").length).toEqual(1);
        expect($(toolboxSelector + " #control_connect").length).toEqual(1);
      });
    });
    
    describe('checking the menubar', function() {
      
      it('should append the menubar', function() {
        expect($("#contentDiv #menubar").length).toEqual(1);
      });
      
      it('should contain a menu for the node shapes', function() {
        var menuSelector = "#contentDiv #menubar #nodeshapermenu";
        expect($(menuSelector).length).toEqual(1);
        expect($(menuSelector +  " #control_none").length).toEqual(1);
        expect($(menuSelector +  " #control_circle").length).toEqual(1);
        expect($(menuSelector +  " #control_rect").length).toEqual(1);
        expect($(menuSelector +  " #control_label").length).toEqual(1);
        expect($(menuSelector +  " #control_singlecolour").length).toEqual(1);
        expect($(menuSelector +  " #control_attributecolour").length).toEqual(1);
        expect($(menuSelector +  " #control_expandcolour").length).toEqual(1);
      });
      
      it('should contain a menu for the edge shapes', function() {
        var menuSelector = "#contentDiv #menubar #edgeshapermenu";
        expect($(menuSelector).length).toEqual(1);
        expect($(menuSelector + " #control_none").length).toEqual(1);
        expect($(menuSelector + " #control_arrow").length).toEqual(1);
        expect($(menuSelector + " #control_label").length).toEqual(1);
        expect($(menuSelector + " #control_singlecolour").length).toEqual(1);
        expect($(menuSelector + " #control_attributecolour").length).toEqual(1);
        expect($(menuSelector + " #control_gradientcolour").length).toEqual(1);
      });
      
      it('should contain a menu for the adapter', function() {
        var menuSelector = "#contentDiv #menubar #adaptermenu";
        expect($(menuSelector).length).toEqual(1);
        
        expect(false).toBeTruthy();
      });
      
    });
    
  });
  
}());
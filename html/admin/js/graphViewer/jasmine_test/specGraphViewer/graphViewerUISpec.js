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
      this.addMatchers({
        
        toBeADropdownMenu: function() {
          var div = this.actual,
            btn = div.children[0],
            list = div.children[1],
            msg = "";
          this.message = function() {
            return msg;
          };
          if (div.className !== "dropdown") {
            msg = "div class to be dropdown";
            return false;
          }
          // Check the toggle button
          if (btn === undefined || btn.tagName.toLowerCase() !== "a") {
            msg = "first element has to be a link";
            return false;
          }
          if (btn.className !== "dropdown-toggle") {
            msg = "first elements class to be dropdown-toggle";
            return false;
          }
          if (btn.role !== "button") {
            msg = "first elements role to be button";
            return false;
          }
          if (btn["data-toggle"] !== "dropdown") {
            msg = "first elements data-toggle to be dropdown";
            return false;
          }
          if (btn["data-target"] !== "#") {
            msg = "first elements data-target to be a link";
            return false;
          }
          if (btn.children[0].tagName.toLowerCase() !== "b" && btn.children[0].className !== "caret") {
            msg = "first element to contain a caret";
            return false;
          }

          // Check the list
          if (list.className !== "dropdown-menu") {
            msg = "list element to be of class dropdown-menu";
            return false;
          }
          if (list.role !== "menu") {
            msg = "list elements role to be menu";
            return false;
          }
          if (list["aria-labelledby"] !== btn.id) {
            msg = "list elements aria-labelledby to be same as button id";
            return false;
          }
          return true;
        }
      });
    });
    
    afterEach(function() {
      document.body.removeChild(div);
    });
    
    it('should throw an error if no container element is given', function() {
      expect(
        function() {
          var t = new GraphViewerUI();
        }
      ).toThrow("A parent element has to be given.");
    });
    
    it('should throw an error if the container element has no id', function() {
      expect(
        function() {
          var t = new GraphViewerUI(document.createElement("div"));
        }
      ).toThrow("The parent element needs an unique id.");
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
        expect($(menuSelector)[0]).toBeADropdownMenu();
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
        expect($(menuSelector)[0]).toBeADropdownMenu();
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
        expect($(menuSelector)[0]).toBeADropdownMenu();
        expect(false).toBeTruthy();
      });
      
    });
    
  });
  
}());
/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect*/
/*global runs, waitsFor, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global ForceLayouter, LayouterControls*/

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

  describe('Layouter UI', function () {
    var layouter, layouterUI, list;

    beforeEach(function () {
      var config = {
        nodes: [],
        links: []
      };
      layouter = new ForceLayouter(config);
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_layout_list";
      layouterUI = new LayouterControls(list, layouter);
      spyOn(layouter, 'changeTo');
      this.addMatchers({
        toBeTag: function(name) {
          var el = this.actual;
          this.message = function() {
            return "Expected " + el.tagName.toLowerCase() + " to be a " + name; 
          };
          return el.tagName.toLowerCase() === name;
        },
        
        toConformToListCSS: function() {
          var li = this.actual,
            a = li.firstChild,
            lbl = a.firstChild;
          expect(li).toBeTag("li");
          expect(a).toBeTag("a");
          expect(lbl).toBeTag("label");
          return true;
        }
      });
    });

    afterEach(function () {
      document.body.removeChild(list);
    });

    it('should throw errors if not setup correctly', function() {
      expect(function() {
        var e = new LayouterControls();
      }).toThrow("A list element has to be given.");
      expect(function() {
        var e = new LayouterControls(list);
      }).toThrow("The Layouter has to be given.");
    });
    
    it('should be able to add a change gravity control to the list', function() {
      runs(function() {
        layouterUI.addControlGravity();
      
        expect($("#control_layout_list #control_layout_gravity").length).toEqual(1);
        expect($("#control_layout_list #control_layout_gravity")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_layout_gravity");
        
        expect($("#control_layout_gravity_modal").length).toEqual(1);
        
        $("#control_layout_gravity_value").attr("value", 42);
        helper.simulateMouseEvent("click", "control_layout_gravity_submit");
      
        expect(layouter.changeTo).toHaveBeenCalledWith({
          gravity: "42"
        });
        
      });
      
      waitsFor(function() {
        return $("#control_layout_gravity_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");    
    });
    
    it('should be able to add a change distance control to the list', function() {
      runs(function() {
        layouterUI.addControlDistance();
      
        expect($("#control_layout_list #control_layout_distance").length).toEqual(1);
        expect($("#control_layout_list #control_layout_distance")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_layout_distance");
        
        expect($("#control_layout_distance_modal").length).toEqual(1);
        
        $("#control_layout_distance_value").attr("value", 42);
        helper.simulateMouseEvent("click", "control_layout_distance_submit");
      
        expect(layouter.changeTo).toHaveBeenCalledWith({
          distance: "42"
        });
        
      });
      
      waitsFor(function() {
        return $("#control_layout_distance_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");    
    });
    
    it('should be able to add a change charge control to the list', function() {
      runs(function() {
        layouterUI.addControlCharge();
      
        expect($("#control_layout_list #control_layout_charge").length).toEqual(1);
        expect($("#control_layout_list #control_layout_charge")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_layout_charge");
        
        expect($("#control_layout_charge_modal").length).toEqual(1);
        
        $("#control_layout_charge_value").attr("value", 42);
        helper.simulateMouseEvent("click", "control_layout_charge_submit");
      
        expect(layouter.changeTo).toHaveBeenCalledWith({
          charge: "42"
        });
        
      });
      
      waitsFor(function() {
        return $("#control_layout_charge_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");    
    });
    
    it('should be able to add all controls to the list', function () {
      layouterUI.addAll();
      
      expect($("#control_layout_list #control_layout_gravity").length).toEqual(1);
      expect($("#control_layout_list #control_layout_distance").length).toEqual(1);
      expect($("#control_layout_list #control_layout_charge").length).toEqual(1);
      
    });
  });

}());
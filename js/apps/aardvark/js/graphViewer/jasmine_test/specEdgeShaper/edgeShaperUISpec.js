/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect*/
/*global runs, waitsFor, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global EdgeShaper, EdgeShaperControls*/

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

  describe('Edge Shaper UI', function () {
    var svg, shaper, shaperUI, list;

    beforeEach(function () {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
      shaper = new EdgeShaper(d3.select("svg"));
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_edge_list";
      shaperUI = new EdgeShaperControls(list, shaper);
      spyOn(shaper, 'changeTo');
      this.addMatchers({
        toConformToListCSS: function() {
          var li = this.actual,
            a = li.firstChild,
            lbl = a.firstChild,
            msg = "";
          this.message = function() {
            return "Expected " + msg;
          };
          if (li === undefined || li.tagName.toLowerCase() !== "li") {
            msg = "first element to be a li";
            return false;
          }
          if (a === undefined || a.tagName.toLowerCase() !== "a") {
            msg = "first element to be a a";
            return false;
          }
          if (lbl === undefined || lbl.tagName.toLowerCase() !== "label") {
            msg = "first element to be a label";
            return false;
          }
          return true;
        }
      });
    });

    afterEach(function () {
      document.body.removeChild(svg);
      document.body.removeChild(list);
    });

    it('should throw errors if not setup correctly', function() {
      expect(function() {
        var e = new EdgeShaperControls();
      }).toThrow("A list element has to be given.");
      expect(function() {
        var e = new EdgeShaperControls(list);
      }).toThrow("The EdgeShaper has to be given.");
    });
    
    it('should be able to add a shape none control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticShapeNone();
      
        expect($("#control_edge_list #control_edge_none").length).toEqual(1);
        expect($("#control_edge_list #control_edge_none")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_edge_none");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: EdgeShaper.shapes.NONE
          }
        });
      });      
    });
    
    it('should be able to add a shape arrow control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticShapeArrow();
      
        expect($("#control_edge_list #control_edge_arrow").length).toEqual(1);
        expect($("#control_edge_list #control_edge_arrow")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_edge_arrow");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: EdgeShaper.shapes.ARROW
          }
        });
      });
            
    });
    
    it('should be able to add a switch label control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticLabel();
      
        expect($("#control_edge_list #control_edge_label").length).toEqual(1);
        expect($("#control_edge_list #control_edge_label")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_edge_label");
        $("#control_edge_label_key").attr("value", "theAnswer");
        helper.simulateMouseEvent("click", "control_edge_label_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          label: "theAnswer"
        });
      });
      
      waitsFor(function() {
        return $("#control_edge_label_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch single colour control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticSingleColour();
      
        expect($("#control_edge_list #control_edge_singlecolour").length).toEqual(1);
        expect($("#control_edge_list #control_edge_singlecolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_edge_singlecolour");
        $("#control_edge_singlecolour_stroke").attr("value", "#123456");
        helper.simulateMouseEvent("click", "control_edge_singlecolour_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "single",
            stroke: "#123456"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_edge_singlecolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch colour on attribute control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticAttributeColour();
      
        expect($("#control_edge_list #control_edge_attributecolour").length).toEqual(1);
        expect($("#control_edge_list #control_edge_attributecolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_edge_attributecolour");
        $("#control_edge_attributecolour_key").attr("value", "label");
        helper.simulateMouseEvent("click", "control_edge_attributecolour_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "attribute",
            key: "label"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_edge_attributecolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch colour to gradient control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticGradientColour();
      
        expect($("#control_edge_list #control_edge_gradientcolour").length).toEqual(1);
        expect($("#control_edge_list #control_edge_gradientcolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_edge_gradientcolour");
        $("#control_edge_gradientcolour_source").attr("value", "#123456");
        $("#control_edge_gradientcolour_target").attr("value", "#654321");
        helper.simulateMouseEvent("click", "control_edge_gradientcolour_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "gradient",
            source: "#123456",
            target: "#654321"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_edge_gradientcolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add all optic controls to the list', function () {
      shaperUI.addAllOptics();
      
      expect($("#control_edge_list #control_edge_none").length).toEqual(1);
      expect($("#control_edge_list #control_edge_arrow").length).toEqual(1);
      expect($("#control_edge_list #control_edge_label").length).toEqual(1);
      expect($("#control_edge_list #control_edge_singlecolour").length).toEqual(1);
      expect($("#control_edge_list #control_edge_attributecolour").length).toEqual(1);
      expect($("#control_edge_list #control_edge_gradientcolour").length).toEqual(1);
      
    });
    
    it('should be able to add all action controls to the list', function () {
      shaperUI.addAllActions();
      
    });
    
    it('should be able to add all controls to the list', function () {
      shaperUI.addAll();
      
      expect($("#control_edge_list #control_edge_none").length).toEqual(1);
      expect($("#control_edge_list #control_edge_arrow").length).toEqual(1);
      expect($("#control_edge_list #control_edge_label").length).toEqual(1);
      expect($("#control_edge_list #control_edge_singlecolour").length).toEqual(1);
      expect($("#control_edge_list #control_edge_attributecolour").length).toEqual(1);
      expect($("#control_edge_list #control_edge_gradientcolour").length).toEqual(1);
      
    });
  });

}());
/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect*/
/*global runs, waitsFor, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global NodeShaper, NodeShaperControls*/

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

  describe('Node Shaper UI', function () {
    var svg, shaper, shaperUI, list;

    beforeEach(function () {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
      shaper = new NodeShaper(d3.select("svg"));
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_list";
      shaperUI = new NodeShaperControls(list, shaper);
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
        var e = new NodeShaperControls();
      }).toThrow("A list element has to be given.");
      expect(function() {
        var e = new NodeShaperControls(list);
      }).toThrow("The NodeShaper has to be given.");
    });
    
    it('should be able to add a shape none control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticShapeNone();
      
        expect($("#control_list #control_none").length).toEqual(1);
        expect($("#control_list #control_none")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_none");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: NodeShaper.shapes.NONE
          }
        });
      });      
    });
    
    it('should be able to add a shape circle control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticShapeCircle();
      
        expect($("#control_list #control_circle").length).toEqual(1);
        expect($("#control_list #control_circle")[0]).toConformToListCSS();
        
        helper.simulateMouseEvent("click", "control_circle");
        expect($("#control_circle_modal").length).toEqual(1);
      
        $("#control_circle_radius").attr("value", 42);
        helper.simulateMouseEvent("click", "control_circle_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: NodeShaper.shapes.CIRCLE,
            radius: "42"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_circle_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a shape rect control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticShapeRect();
      
        expect($("#control_list #control_rect").length).toEqual(1);
        expect($("#control_list #control_rect")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_rect");
        $("#control_rect_width").attr("value", 42);
        $("#control_rect_height").attr("value", 12);
        helper.simulateMouseEvent("click", "control_rect_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: NodeShaper.shapes.RECT,
            width: "42",
            height: "12"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_rect_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch label control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticLabel();
      
        expect($("#control_list #control_label").length).toEqual(1);
        expect($("#control_list #control_label")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_label");
        $("#control_label_key").attr("value", "theAnswer");
        helper.simulateMouseEvent("click", "control_label_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          label: "theAnswer"
        });
      });
      
      waitsFor(function() {
        return $("#control_label_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch single colour control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticSingleColour();
      
        expect($("#control_list #control_singlecolour").length).toEqual(1);
        expect($("#control_list #control_singlecolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_singlecolour");
        $("#control_singlecolour_fill").attr("value", "#123456");
        $("#control_singlecolour_stroke").attr("value", "#654321");
        helper.simulateMouseEvent("click", "control_singlecolour_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "single",
            fill: "#123456",
            stroke: "#654321"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_singlecolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch colour on attribute control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticAttributeColour();
      
        expect($("#control_list #control_attributecolour").length).toEqual(1);
        expect($("#control_list #control_attributecolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_attributecolour");
        $("#control_attributecolour_key").attr("value", "label");
        helper.simulateMouseEvent("click", "control_attributecolour_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "attribute",
            key: "label"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_attributecolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch colour on expand status control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticExpandColour();
      
        expect($("#control_list #control_expandcolour").length).toEqual(1);
        expect($("#control_list #control_expandcolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_expandcolour");
        $("#control_expandcolour_expanded").attr("value", "#123456");
        $("#control_expandcolour_collapsed").attr("value", "#654321");
        helper.simulateMouseEvent("click", "control_expandcolour_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "expand",
            expanded: "#123456",
            collapsed: "#654321"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_expandcolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    
    it('should be able to add all optic controls to the list', function () {
      shaperUI.addAllOptics();
      
      expect($("#control_list #control_none").length).toEqual(1);
      expect($("#control_list #control_circle").length).toEqual(1);
      expect($("#control_list #control_rect").length).toEqual(1);
      expect($("#control_list #control_label").length).toEqual(1);
      expect($("#control_list #control_singlecolour").length).toEqual(1);
      expect($("#control_list #control_attributecolour").length).toEqual(1);
      expect($("#control_list #control_expandcolour").length).toEqual(1);
    });
    
    it('should be able to add all action controls to the list', function () {
      shaperUI.addAllActions();
      
    });
    
    it('should be able to add all controls to the list', function () {
      shaperUI.addAll();
      
      expect($("#control_list #control_none").length).toEqual(1);
      expect($("#control_list #control_circle").length).toEqual(1);
      expect($("#control_list #control_rect").length).toEqual(1);
      expect($("#control_list #control_label").length).toEqual(1);
      expect($("#control_list #control_singlecolour").length).toEqual(1);
      expect($("#control_list #control_attributecolour").length).toEqual(1);
      expect($("#control_list #control_expandcolour").length).toEqual(1);
    });
  });

}());
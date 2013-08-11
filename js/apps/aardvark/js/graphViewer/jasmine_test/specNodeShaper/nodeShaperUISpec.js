/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine*/
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
      list.id = "control_node_list";
      shaperUI = new NodeShaperControls(list, shaper);
      spyOn(shaper, 'changeTo');
      spyOn(shaper, 'getColourMapping').andCallFake(function() {
        return {
          blue: {
            list: ["bl", "ue"],
            front: "white"
          },
          green: {
            list: ["gr", "een"],
            front: "black"
          }
        };
      });
      spyOn(shaper, "setColourMappingListener");
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
      
        expect($("#control_node_list #control_node_none").length).toEqual(1);
        expect($("#control_node_list #control_node_none")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_node_none");
      
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
      
        expect($("#control_node_list #control_node_circle").length).toEqual(1);
        expect($("#control_node_list #control_node_circle")[0]).toConformToListCSS();
        
        helper.simulateMouseEvent("click", "control_node_circle");
        expect($("#control_node_circle_modal").length).toEqual(1);
      
        $("#control_node_circle_radius").attr("value", 42);
        helper.simulateMouseEvent("click", "control_node_circle_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: NodeShaper.shapes.CIRCLE,
            radius: "42"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_node_circle_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a shape rect control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticShapeRect();
      
        expect($("#control_node_list #control_node_rect").length).toEqual(1);
        expect($("#control_node_list #control_node_rect")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_node_rect");
        $("#control_node_rect_width").attr("value", 42);
        $("#control_node_rect_height").attr("value", 12);
        helper.simulateMouseEvent("click", "control_node_rect_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: NodeShaper.shapes.RECT,
            width: "42",
            height: "12"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_node_rect_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch label control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticLabel();
      
        expect($("#control_node_list #control_node_label").length).toEqual(1);
        expect($("#control_node_list #control_node_label")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_node_label");
        $("#control_node_label_key").attr("value", "theAnswer");
        helper.simulateMouseEvent("click", "control_node_label_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          label: "theAnswer"
        });
      });
      
      waitsFor(function() {
        return $("#control_node_label_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch single colour control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticSingleColour();
      
        expect($("#control_node_list #control_node_singlecolour").length).toEqual(1);
        expect($("#control_node_list #control_node_singlecolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_node_singlecolour");
        $("#control_node_singlecolour_fill").attr("value", "#123456");
        $("#control_node_singlecolour_stroke").attr("value", "#654321");
        helper.simulateMouseEvent("click", "control_node_singlecolour_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "single",
            fill: "#123456",
            stroke: "#654321"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_node_singlecolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch colour on attribute control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticAttributeColour();
      
        expect($("#control_node_list #control_node_attributecolour").length).toEqual(1);
        expect($("#control_node_list #control_node_attributecolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_node_attributecolour");
        $("#control_node_attributecolour_key").attr("value", "label");
        helper.simulateMouseEvent("click", "control_node_attributecolour_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "attribute",
            key: "label"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_node_attributecolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a switch colour on expand status control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticExpandColour();
      
        expect($("#control_node_list #control_node_expandcolour").length).toEqual(1);
        expect($("#control_node_list #control_node_expandcolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_node_expandcolour");
        $("#control_node_expandcolour_expanded").attr("value", "#123456");
        $("#control_node_expandcolour_collapsed").attr("value", "#654321");
        helper.simulateMouseEvent("click", "control_node_expandcolour_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "expand",
            expanded: "#123456",
            collapsed: "#654321"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_node_expandcolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add all optic controls to the list', function () {
      shaperUI.addAllOptics();
      
      expect($("#control_node_list #control_node_none").length).toEqual(1);
      expect($("#control_node_list #control_node_circle").length).toEqual(1);
      expect($("#control_node_list #control_node_rect").length).toEqual(1);
      expect($("#control_node_list #control_node_label").length).toEqual(1);
      expect($("#control_node_list #control_node_singlecolour").length).toEqual(1);
      expect($("#control_node_list #control_node_attributecolour").length).toEqual(1);
      expect($("#control_node_list #control_node_expandcolour").length).toEqual(1);
    });
    
    it('should be able to add all action controls to the list', function () {
      shaperUI.addAllActions();
      
    });
    
    it('should be able to add all controls to the list', function () {
      shaperUI.addAll();
      
      expect($("#control_node_list #control_node_none").length).toEqual(1);
      expect($("#control_node_list #control_node_circle").length).toEqual(1);
      expect($("#control_node_list #control_node_rect").length).toEqual(1);
      expect($("#control_node_list #control_node_label").length).toEqual(1);
      expect($("#control_node_list #control_node_singlecolour").length).toEqual(1);
      expect($("#control_node_list #control_node_attributecolour").length).toEqual(1);
      expect($("#control_node_list #control_node_expandcolour").length).toEqual(1);
    });
    
    it('should be able to create a div containing the color <-> label mapping', function() {
      var div = shaperUI.createColourMappingList(),
        list = div.firstChild,
        blue = list.children[0],
        green = list.children[1],
        blue1 = blue.children[0],
        blue2 = blue.children[1],
        green1 = green.children[0],
        green2 = green.children[1];

      expect(shaper.getColourMapping).wasCalled();
      expect(shaper.setColourMappingListener).wasCalledWith(jasmine.any(Function));
      expect(div).toBeTag("div");
      expect($(div).attr("id")).toEqual("node_colour_list");
      expect(list).toBeTag("ul");
      expect(blue).toBeTag("ul");
      expect(blue.children.length).toEqual(2);
      expect(blue.style.backgroundColor).toEqual("blue");
      
      expect(blue1).toBeTag("li");
      expect($(blue1).text()).toEqual("bl");
      expect(blue2).toBeTag("li");
      expect($(blue2).text()).toEqual("ue");
      
      expect(green).toBeTag("ul");
      expect(green.children.length).toEqual(2);
      expect(green.style.backgroundColor).toEqual("green");
      
      expect(green1).toBeTag("li");
      expect($(green1).text()).toEqual("gr");
      expect(green2).toBeTag("li");
      expect($(green2).text()).toEqual("een");
    
    });
    
    
    it('should be able to submit the controls with return', function() {
      runs(function() {
        shaperUI.addControlOpticExpandColour();
      
        expect($("#control_node_list #control_node_expandcolour").length).toEqual(1);
        expect($("#control_node_list #control_node_expandcolour")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_node_expandcolour");
        $("#control_node_expandcolour_expanded").attr("value", "#123456");
        $("#control_node_expandcolour_collapsed").attr("value", "#654321");
        
        helper.simulateReturnEvent();
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: "expand",
            expanded: "#123456",
            collapsed: "#654321"
          }
        });
      });
      
      waitsFor(function() {
        return $("#control_node_expandcolour_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });    
    
    describe('checking to change colour and label at once', function() {
      
      it('should be able to add the control and create the mapping list', function() {
        runs(function() {
          shaperUI.addControlOpticLabelAndColour();
          spyOn(shaperUI, "createColourMappingList").andCallThrough();
          
          expect($("#control_node_list #control_node_labelandcolour").length).toEqual(1);
          expect($("#control_node_list #control_node_labelandcolour")[0]).toConformToListCSS();
      
          helper.simulateMouseEvent("click", "control_node_labelandcolour");
          $("#control_node_labelandcolour_label-attribute").attr("value", "label");
          helper.simulateMouseEvent("click", "control_node_labelandcolour_submit");
          
          expect(shaper.changeTo).toHaveBeenCalledWith({
            label: "label",
            color: {
              type: "attribute",
              key: "label"
            }
          });
          expect(shaperUI.createColourMappingList).wasCalled();
          expect(shaper.setColourMappingListener).wasCalledWith(jasmine.any(Function));
        });
      
        waitsFor(function() {
          return $("#control_node_attributecolour_modal").length === 0;
        }, 2000, "The modal dialog should disappear.");
      
        runs(function() {
          expect(true).toBeTruthy();
        });
      
      });      
    });
  });

}());

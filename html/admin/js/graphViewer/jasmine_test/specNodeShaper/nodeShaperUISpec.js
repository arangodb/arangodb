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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


(function () {
  "use strict";

  describe('Node Shaper UI', function () {
    var svg, shaper, shaperUI, list, spy;

    beforeEach(function () {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
      shaper = new NodeShaper(d3.select("svg"));
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_list";
      shaperUI = new NodeShaperControls(list, shaper);
      spyOn(shaper, 'changeTo');
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
    
    it('should be able to add a shape circle control to the list', function() {
      runs(function() {
        shaperUI.addControlOpticShapeCircle();
      
        expect($("#control_list #control_circle").length).toEqual(1);
      
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
      shaperUI.addControlOpticShapeRect();
      
      expect($(".control_list #control_rect").length).toEqual(1);
      
      helper.simulateMouseEvent("click", "control_rect");
      $("control_rect_width").attr("value", 42);
      $("control_rect_height").attr("value", 12);
      helper.simulateMouseEvent("click", "control_rect_confirm");
      
      expect(shaper.changeTo).toHaveBeenCalledWith({
        shape: {
          type: NodeShaper.shapes.RECT,
          width: 42,
          height: 12
        }
      });
      
    });
    
    it('should be able to add a switch label control to the list', function() {
      shaperUI.addControlOpticLabel();
      
      expect($(".control_list #control_label").length).toEqual(1);
      
      helper.simulateMouseEvent("click", "control_label");
      $("control_label_key").attr("value", "theAnswer");
      helper.simulateMouseEvent("click", "control_label_confirm");
      
      expect(shaper.changeTo).toHaveBeenCalledWith({
        label: "theAnswer"
      });
      
    });
    
    it('should be able to add all optic controls to the list', function () {
      shaperUI.addAllOptics();
      
      expect($(".control_list #control_circle").length).toEqual(1);
      expect($(".control_list #control_rect").length).toEqual(1);
      expect($(".control_list #control_label").length).toEqual(1);
      
    });
    
    it('should be able to add all action controls to the list', function () {
      shaperUI.addAllActions();
      
    });
    
    it('should be able to add all controls to the list', function () {
      shaperUI.addAll();
      
      expect($(".control_list #control_circle").length).toEqual(1);
      expect($(".control_list #control_rect").length).toEqual(1);
      expect($(".control_list #control_label").length).toEqual(1);
      
    });
  });

}());
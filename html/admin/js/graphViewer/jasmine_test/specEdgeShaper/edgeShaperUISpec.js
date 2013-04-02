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
    var svg, shaper, shaperUI, list, spy;

    beforeEach(function () {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
      shaper = new EdgeShaper(d3.select("svg"));
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_list";
      shaperUI = new EdgeShaperControls(list, shaper);
      spyOn(shaper, 'changeTo');
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
      
        expect($("#control_list #control_none").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_none");
      
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
      
        expect($("#control_list #control_arrow").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_arrow");
      
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
      
        expect($("#control_list #control_label").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_label");
        $("#control_label_key").attr("value", "theAnswer");
        helper.simulateMouseEvent("click", "control_label_submit");
      
        expect(shaper.changeTo).toHaveBeenCalledWith({
          label: "theAnswer"
        });
      });
      
      waitsFor(function() {
        return $("#control_rect_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add all optic controls to the list', function () {
      shaperUI.addAllOptics();
      
      expect($("#control_list #control_none").length).toEqual(1);
      expect($("#control_list #control_arrow").length).toEqual(1);
      expect($("#control_list #control_label").length).toEqual(1);
      
    });
    
    it('should be able to add all action controls to the list', function () {
      shaperUI.addAllActions();
      
    });
    
    it('should be able to add all controls to the list', function () {
      shaperUI.addAll();
      
      expect($("#control_list #control_none").length).toEqual(1);
      expect($("#control_list #control_arrow").length).toEqual(1);
      expect($("#control_list #control_label").length).toEqual(1);
      
    });
  });

}());
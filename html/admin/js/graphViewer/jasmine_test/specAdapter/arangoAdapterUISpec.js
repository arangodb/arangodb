/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect*/
/*global runs, waitsFor, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global ArangoAdapterControls */

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

  describe('Arango Adapter UI', function () {
    var adapter, adapterUI, list;

    beforeEach(function () {
      adapter = {
        
      };
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_list";
      adapterUI = new ArangoAdapterControls(list, adapter);
      spyOn(adapter, 'changeTo');

    });

    afterEach(function () {
      document.body.removeChild(list);
    });

    it('should throw errors if not setup correctly', function() {
      expect(function() {
        var e = new ArangoAdapterControls();
      }).toThrow("A list element has to be given.");
      expect(function() {
        var e = new ArangoAdapterControls(list);
      }).toThrow("The ArangoAdapter has to be given.");
    });
    
    it('should be able to add a change collections control to the list', function() {
      runs(function() {
        adapterUI.addControlChangeCollections();
      
        expect($("#control_list #control_collections").length).toEqual(1);
        expect($("#control_list #control_collections")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_collections");
      
        expect($("#control_collections_modal").length).toEqual(1);
      
        $("#control_collections_nodes").attr("value", "newNodes");
        $("#control_collections_edges").attr("value", "newEdges");
        
        helper.simulateMouseEvent("click", "control_collections_submit");
      
        expect(adapter.changeTo).toHaveBeenCalledWith(
          "newNodes",
          "newEdges"
        );
      });      
    });
  });
}());
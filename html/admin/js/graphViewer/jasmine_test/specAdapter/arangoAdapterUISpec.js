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
        changeTo: function(){}
      };
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_adapter_list";
      adapterUI = new ArangoAdapterControls(list, adapter);
      spyOn(adapter, 'changeTo');
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
        var e = new ArangoAdapterControls();
      }).toThrow("A list element has to be given.");
      expect(function() {
        var e = new ArangoAdapterControls(list);
      }).toThrow("The ArangoAdapter has to be given.");
    });
    
    it('should be able to add a change collections control to the list', function() {
      runs(function() {
        adapterUI.addControlChangeCollections();
      
        expect($("#control_adapter_list #control_adapter_collections").length).toEqual(1);
        expect($("#control_adapter_list #control_adapter_collections")[0]).toConformToListCSS();
      
        helper.simulateMouseEvent("click", "control_adapter_collections");
      
        expect($("#control_adapter_collections_modal").length).toEqual(1);
      
        $("#control_adapter_collections_nodecollection").attr("value", "newNodes");
        $("#control_adapter_collections_edgecollection").attr("value", "newEdges");
        
        helper.simulateMouseEvent("click", "control_adapter_collections_submit");
      
        expect(adapter.changeTo).toHaveBeenCalledWith(
          "newNodes",
          "newEdges",
          false
        );
        
      });
      
      waitsFor(function() {
        return $("#control_adapter_collections_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
          
    });
    
    it('should change collections and traversal direction to directed', function() {
      runs(function() {
        adapterUI.addControlChangeCollections();
        helper.simulateMouseEvent("click", "control_adapter_collections");

        $("#control_adapter_collections_nodecollection").attr("value", "newNodes");
        $("#control_adapter_collections_edgecollection").attr("value", "newEdges");
        $("#control_adapter_collections_undirected").attr("checked", false);
        
        helper.simulateMouseEvent("click", "control_adapter_collections_submit");
      
        expect(adapter.changeTo).toHaveBeenCalledWith(
          "newNodes",
          "newEdges",
          false
        );
        
      });
      
      waitsFor(function() {
        return $("#control_adapter_collections_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
          
    });
    
    it('should change collections and traversal direction to undirected', function() {
      runs(function() {
        adapterUI.addControlChangeCollections();      
        helper.simulateMouseEvent("click", "control_adapter_collections");
        $("#control_adapter_collections_nodecollection").attr("value", "newNodes");
        $("#control_adapter_collections_edgecollection").attr("value", "newEdges");
        $("#control_adapter_collections_undirected").attr("checked", true);
        
        helper.simulateMouseEvent("click", "control_adapter_collections_submit");
      
        expect(adapter.changeTo).toHaveBeenCalledWith(
          "newNodes",
          "newEdges",
          true
        );
        
      });
      
      waitsFor(function() {
        return $("#control_adapter_collections_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
          
    });
    
    it('should be able to add all controls to the list', function() {
      adapterUI.addAll();
      expect($("#control_adapter_list #control_adapter_collections").length).toEqual(1);
    });
  });
}());
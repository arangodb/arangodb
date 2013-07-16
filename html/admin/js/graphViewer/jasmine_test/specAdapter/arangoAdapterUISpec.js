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
    var adapter, adapterUI, list, prioList;

    beforeEach(function () {
      prioList = [];
      adapter = {
        getPrioList: function(){},
        changeTo: function(){},
        changeToCollections: function(){},
        getCollections: function(cb) {
          cb(["nodes", "newNodes"], ["edges", "newEdges"]);
        }
      };
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_adapter_list";
      adapterUI = new ArangoAdapterControls(list, adapter);
      spyOn(adapter, 'changeTo');
      spyOn(adapter, 'changeToCollections');
      spyOn(adapter, "getCollections").andCallThrough();
      spyOn(adapter, "getPrioList").andCallFake(function() {
        return prioList;
      });
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
    
    describe('change collections control', function() {
      
      beforeEach(function() {
        adapterUI.addControlChangeCollections();
        
        expect($("#control_adapter_list #control_adapter_collections").length).toEqual(1);
        expect($("#control_adapter_list #control_adapter_collections")[0]).toConformToListCSS();
        helper.simulateMouseEvent("click", "control_adapter_collections");
        expect($("#control_adapter_collections_modal").length).toEqual(1);
      });
      
      afterEach(function() {
        waitsFor(function() {
          return $("#control_adapter_collections_modal").length === 0;
        }, 2000, "The modal dialog should disappear.");
      });
      
      it('should be added to the list', function() {
        runs(function() {
          $("#control_adapter_collections_nodecollection").attr("value", "newNodes");
          $("#control_adapter_collections_edgecollection").attr("value", "newEdges");
          helper.simulateMouseEvent("click", "control_adapter_collections_submit");
          expect(adapter.changeToCollections).toHaveBeenCalledWith(
            "newNodes",
            "newEdges",
            false
          );
        });
      });
      
      it('should change collections and traversal direction to directed', function() {
        runs(function() {

          $("#control_adapter_collections_nodecollection").attr("value", "newNodes");
          $("#control_adapter_collections_edgecollection").attr("value", "newEdges");
          $("#control_adapter_collections_undirected").attr("checked", false);
        
          helper.simulateMouseEvent("click", "control_adapter_collections_submit");
      
          expect(adapter.changeToCollections).toHaveBeenCalledWith(
            "newNodes",
            "newEdges",
            false
          );
        });
      
      });
      
      it('should change collections and traversal direction to undirected', function() {
        runs(function() {
          $("#control_adapter_collections_nodecollection").attr("value", "newNodes");
          $("#control_adapter_collections_edgecollection").attr("value", "newEdges");
          $("#control_adapter_collections_undirected").attr("checked", true);
        
          helper.simulateMouseEvent("click", "control_adapter_collections_submit");
      
          expect(adapter.changeToCollections).toHaveBeenCalledWith(
            "newNodes",
            "newEdges",
            true
          );
        });
      });
      
      it('should offer the available collections as lists', function() {
        runs(function() {
          var docList = document.getElementById("control_adapter_collections_nodecollection"),
            edgeList = document.getElementById("control_adapter_collections_edgecollection"),
            docCollectionOptions = docList.children,
            edgeCollectionOptions = edgeList.children;
           
          expect(adapter.getCollections).toHaveBeenCalled();
        
          expect(docList).toBeTag("select");
          expect(docCollectionOptions.length).toEqual(2);
          expect(docCollectionOptions[0]).toBeTag("option");
          expect(docCollectionOptions[1]).toBeTag("option");
          expect(docCollectionOptions[0].value).toEqual("nodes");
          expect(docCollectionOptions[1].value).toEqual("newNodes");
        
        
          expect(edgeList).toBeTag("select");
          expect(edgeCollectionOptions.length).toEqual(2);
          expect(edgeCollectionOptions[0]).toBeTag("option");
          expect(edgeCollectionOptions[1]).toBeTag("option");
          expect(edgeCollectionOptions[0].value).toEqual("edges");
          expect(edgeCollectionOptions[1].value).toEqual("newEdges");
        
          helper.simulateMouseEvent("click", "control_adapter_collections_submit");
        });
      });
    });

    describe('change priority list control', function() {
      beforeEach(function() {
        adapterUI.addControlChangePriority();
        
        expect($("#control_adapter_list #control_adapter_priority").length).toEqual(1);
        expect($("#control_adapter_list #control_adapter_priority")[0]).toConformToListCSS();
        helper.simulateMouseEvent("click", "control_adapter_priority");
        expect($("#control_adapter_priority_modal").length).toEqual(1);
      });
      
      afterEach(function() {
        waitsFor(function() {
          return $("#control_adapter_priority_modal").length === 0;
        }, 2000, "The modal dialog should disappear.");
      });
      
      it('should be added to the list', function() {
        runs(function() {
          $("#control_adapter_priority_attribute_1").attr("value", "foo");
          helper.simulateMouseEvent("click", "control_adapter_priority_submit");
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: ["foo"]
          });
        });
      });
      
      it('should not add empty attributes to priority', function() {
        runs(function() {
          $("#control_adapter_priority_attribute_1").attr("value", "");
          helper.simulateMouseEvent("click", "control_adapter_priority_submit");
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: []
          });
        });
      });
      
      it('should add a new line to priority on demand', function() {
        runs(function() {
          helper.simulateMouseEvent("click", "control_adapter_priority_attribute_addLine");
          expect($("#control_adapter_priority_attribute_1").length).toEqual(1);
          expect($("#control_adapter_priority_attribute_2").length).toEqual(1);
          expect($("#control_adapter_priority_attribute_addLine").length).toEqual(1);
          $("#control_adapter_priority_attribute_1").attr("value", "foo");
          $("#control_adapter_priority_attribute_2").attr("value", "bar");
          helper.simulateMouseEvent("click", "control_adapter_priority_submit");
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: ["foo", "bar"]
          });
        });
      });
      
      it('should add many new lines to priority on demand', function() {
        runs(function() {
          helper.simulateMouseEvent("click", "control_adapter_priority_attribute_addLine");
          helper.simulateMouseEvent("click", "control_adapter_priority_attribute_addLine");
          helper.simulateMouseEvent("click", "control_adapter_priority_attribute_addLine");
          helper.simulateMouseEvent("click", "control_adapter_priority_attribute_addLine");
          expect($("#control_adapter_priority_attribute_1").length).toEqual(1);
          expect($("#control_adapter_priority_attribute_2").length).toEqual(1);
          expect($("#control_adapter_priority_attribute_3").length).toEqual(1);
          expect($("#control_adapter_priority_attribute_4").length).toEqual(1);
          expect($("#control_adapter_priority_attribute_5").length).toEqual(1);
          expect($("#control_adapter_priority_attribute_addLine").length).toEqual(1);
          $("#control_adapter_priority_attribute_1").attr("value", "foo");
          $("#control_adapter_priority_attribute_2").attr("value", "bar");
          $("#control_adapter_priority_attribute_3").attr("value", "");
          $("#control_adapter_priority_attribute_4").attr("value", "baz");
          $("#control_adapter_priority_attribute_5").attr("value", "foxx");
          helper.simulateMouseEvent("click", "control_adapter_priority_submit");
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: ["foo", "bar", "baz", "foxx"]
          });
        });
      });
      
      it('should remove all but the first line', function() {
        runs(function() {
          helper.simulateMouseEvent("click", "control_adapter_priority_attribute_addLine");
          expect($("#control_adapter_priority_attribute_1_remove").length).toEqual(0);
          expect($("#control_adapter_priority_attribute_2_remove").length).toEqual(1);
          helper.simulateMouseEvent("click", "control_adapter_priority_attribute_2_remove");
          
          expect($("#control_adapter_priority_attribute_addLine").length).toEqual(1);
          expect($("#control_adapter_priority_attribute_2_remove").length).toEqual(0);
          expect($("#control_adapter_priority_attribute_2").length).toEqual(0);
          
          $("#control_adapter_priority_attribute_1").attr("value", "foo");
          helper.simulateMouseEvent("click", "control_adapter_priority_submit");
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: ["foo"]
          });
        });
      });
      
      it('should load the current prioList from the adapter', function() {
        
        runs(function() {
          helper.simulateMouseEvent("click", "control_adapter_priority_cancel");
        });
        
        waitsFor(function() {
          return $("#control_adapter_priority_modal").length === 0;
        }, 2000, "The modal dialog should disappear.");
        
        runs(function() {
          expect($("#control_adapter_priority_cancel").length).toEqual(0);
          prioList.push("foo");
          prioList.push("bar");
          prioList.push("baz");
          helper.simulateMouseEvent("click", "control_adapter_priority");
          expect(adapter.getPrioList).wasCalled();
          var idPrefix = "#control_adapter_priority_attribute_";
          expect($(idPrefix + "1").length).toEqual(1);
          expect($(idPrefix + "2").length).toEqual(1);
          expect($(idPrefix + "3").length).toEqual(1);
          expect($(idPrefix + "4").length).toEqual(0);
          expect($(idPrefix + "addLine").length).toEqual(1);
          expect($(idPrefix + "1_remove").length).toEqual(0);
          expect($(idPrefix + "2_remove").length).toEqual(1);
          expect($(idPrefix + "3_remove").length).toEqual(1);
          expect($(idPrefix + "1").attr("value")).toEqual("foo");
          expect($(idPrefix + "2").attr("value")).toEqual("bar");
          expect($(idPrefix + "3").attr("value")).toEqual("baz");
          expect($("#control_adapter_priority_modal").length).toEqual(1);
          expect($("#control_adapter_priority_cancel").length).toEqual(1);
          helper.simulateMouseEvent("click", "control_adapter_priority_cancel");
        });
      });
      
    });
    
    it('should be able to add all controls to the list', function() {
      adapterUI.addAll();
      expect($("#control_adapter_list #control_adapter_collections").length).toEqual(1);
      expect($("#control_adapter_list #control_adapter_priority").length).toEqual(1);
    });
  });
}());
/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global runs, spyOn, waitsFor */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global JSONAdapter*/

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

  describe('JSON Adapter', function () {
    
    var adapter,
      nodes,
      edges,
      jsonPath,
      startNode,
      
      nodeWithID = function(id) {
        return $.grep(nodes, function(e){
          return e.id === id;
        })[0];
      },
      
      existNode = function(id) {
        var node = nodeWithID(id);
        expect(node).toBeDefined();
        expect(node.id).toEqual(id);
      },
      
      existNodes = function(ids) {
        _.each(ids, existNode);
      };
      
      
    
    beforeEach(function() {
      jsonPath = "../generated_1000/";
      nodes = [];
      edges = [];
      // Helper function to easily insert a node into the list
      nodes.insertNode = function(node) {
        this.push(node);
      };
      // Helper function to easily insert edges in the list
      edges.insertEdge = function(source, target) {
        this.push({source: source, target: target});
      };
      startNode = 477;
      adapter = new JSONAdapter(jsonPath, nodes, edges);
    });
    
    it('should be able to load a tree node form a json file', function() {
      var callbackCheck;
      
      runs(function() {
        callbackCheck = false;
        adapter.loadNodeFromTree(startNode, function() {
          callbackCheck = true;
        });
      });
      
      waitsFor(function() {
        return callbackCheck;
      });
      
      runs(function() {
        existNodes([477, 29, 159, 213, 339]);
        expect(nodes.length).toEqual(5);
      });
    });
    
    it('should be able to request the number of children centrality', function() {
      var callbackCheck,
      children;
      
      runs(function() {
        callbackCheck = false;
        adapter.requestCentralityChildren(startNode, function(count) {
          callbackCheck = true;
          children = count;
        });
      });
      
      waitsFor(function() {
        return callbackCheck;
      });
      
      runs(function() {
        expect(children).toEqual(4);
      });
    });
    
    describe('that has already loaded one file', function() {
      
      beforeEach(function() {
        var callbackCheck;
      
        runs(function() {
          callbackCheck = false;
          adapter.loadNodeFromTree(startNode, function() {
            callbackCheck = true;
          });
        });
      
        waitsFor(function() {
          return callbackCheck;
        });        
      });
      
      it('should be able to add nodes from another file', function() {
        var callbackCheck;
        
        runs(function() {
          callbackCheck = false;
          adapter.loadNodeFromTree(29, function() {
            callbackCheck = true;
          });
        });
      
        waitsFor(function() {
          return callbackCheck;
        });
      
        runs(function() {
          existNodes([477, 29, 159, 213, 339, 3, 7]);
          expect(nodes.length).toEqual(7);
        });
      });
      
    });
    
    describe('that has loaded several files', function() {
      beforeEach(function() {
        var callbackCheck;
      
        runs(function() {
          callbackCheck = false;
          adapter.loadNodeFromTree(startNode, function() {
            callbackCheck = true;
          });
        });
      
        waitsFor(function() {
          return callbackCheck;
        });
      
        runs(function() {
          callbackCheck = false;
          adapter.loadNodeFromTree(29, function() {
            callbackCheck = true;
          });
        });
      
        waitsFor(function() {
          return callbackCheck;
        });
      
        runs(function() {
          callbackCheck = false;
          adapter.loadNodeFromTree(3, function() {
            callbackCheck = true;
          });
        });
      
        waitsFor(function() {
          return callbackCheck;
        });
        
        it('should not add a node to the list twice', function() {
          var callbackCheck;
      
          runs(function() {
            callbackCheck = false;
            adapter.loadNodeFromTree(213, function() {
              callbackCheck = true;
            });
          });
      
          waitsFor(function() {
            return callbackCheck;
          });  
          
          runs(function() {
            existNodes([477, 29, 159, 213, 339, 3, 7, 4, 80]);
            expect(nodes.length).toEqual(9);
          });
        });
      });
    });
  });
  
}());
/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach, jasmine */
/*global describe, it, expect, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global NodeReducer*/

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

  describe('Node Reducer', function () {
    
    describe('setup process', function() {
      
      it('should throw an error if no nodes are given', function() {
        expect(function() {
          var s = new NodeReducer();
        }).toThrow("Nodes have to be given.");
      });
      
      it('should throw an error if no edges are given', function() {
        expect(function() {
          var s = new NodeReducer([]);
        }).toThrow("Edges have to be given.");
      });
      
      it('should not throw an error if mandatory information is given', function() {
        expect(function() {
          var s = new NodeReducer([], []);
        }).not.toThrow();
      });
    });

    describe('setup correctly', function() {
      
      var reducer,
      nodes,
      edges;

      beforeEach(function () {
        nodes = [];
        edges = [];
        reducer = new NodeReducer(nodes, edges);
        this.addMatchers({
          toContainNodes: function(ns) {
            var com = this.actual,
            check = true;
            this.message = function() {
              var msg = _.map(com, function(c) {
                return c._id;
              });
              return "Expected " + msg + " to contain " + ns; 
            };
            if (com.length !== ns.length) {
              return false;
            }
            _.each(ns, function(n) {
              if(!_.contains(com, nodes[n])) {
                check = false;
              }
            });
            return check;
          }
        });
      });
      
      describe('checking the interface', function() {
        
        it('should offer a function to identify a far away community', function() {
          expect(reducer.getCommunity).toBeDefined();
          expect(reducer.getCommunity).toEqual(jasmine.any(Function));
          expect(reducer.getCommunity.length).toEqual(2);
        });
        
      });
      
      describe('checking community identification', function() {
        
        it('should be able to identify an obvious community', function() {
          helper.insertSimpleNodes(nodes, [0, 1, 2, 3, 4]);
          edges.push(helper.createSimpleEdge(nodes, 0, 1));
          edges.push(helper.createSimpleEdge(nodes, 0, 2));
          edges.push(helper.createSimpleEdge(nodes, 0, 3));
          edges.push(helper.createSimpleEdge(nodes, 3, 4));
          
          var com = reducer.getCommunity(2, nodes[4]);
          expect(com).toContainNodes([0, 1, 2, 3]);
        });
        
        it('should prefer cliques as a community over an equal sized other group', function() {
          nodes = helper.createSimpleNodes([0, 1, 2, 3, 4, 5, 6, 7, 8]);
          edges = helper.createClique(nodes, [0, 1, 2, 3]);
          edges.push(helper.createSimpleEdge(nodes, 4, 3));
          edges.push(helper.createSimpleEdge(nodes, 4, 5));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
          edges.push(helper.createSimpleEdge(nodes, 5, 7));
          edges.push(helper.createSimpleEdge(nodes, 5, 8));
          
          var com = reducer.getCommunity(6, nodes[4]);
          expect(com).toContainNodes([0, 1, 2, 3]);
        });
        
        it('should not return a close group if there is an alternative', function() {
          nodes = helper.createSimpleNodes([0, 1, 2, 3, 4, 5, 6, 7]);
          edges = helper.createClique(nodes, [0, 1, 2]);
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          edges.push(helper.createSimpleEdge(nodes, 3, 4));
          edges.push(helper.createSimpleEdge(nodes, 4, 5));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
          edges.push(helper.createSimpleEdge(nodes, 5, 7));
          
          var com = reducer.getCommunity(6, nodes[3]);
          expect(com).toContainNodes([5, 6, 7]);
        });
        
      });
      
    });
  });



}());
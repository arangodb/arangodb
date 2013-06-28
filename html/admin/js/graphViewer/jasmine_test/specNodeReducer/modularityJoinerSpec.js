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

  describe('Modularity Joiner', function () {
    
    describe('setup process', function() {
      
      it('should throw an error if no nodes are given', function() {
        expect(function() {
          var s = new ModularityJoiner();
        }).toThrow("Nodes have to be given.");
      });
      
      it('should throw an error if no edges are given', function() {
        expect(function() {
          var s = new ModularityJoiner([]);
        }).toThrow("Edges have to be given.");
      });
      
      it('should not throw an error if mandatory information is given', function() {
        expect(function() {
          var s = new ModularityJoiner([], []);
        }).not.toThrow();
      });
    });
    
    describe('setup correctly', function() {
      
      var joiner,
      nodes,
      edges;

      beforeEach(function () {
        nodes = [];
        edges = [];
        joiner = new ModularityJoiner(nodes, edges);
      });
      
      describe('checking the interface', function() {
        
        it('should offer a function to compute the Adjacency Matrix', function() {
          expect(joiner.makeAdjacencyMatrix).toBeDefined();
          expect(joiner.makeAdjacencyMatrix).toEqual(jasmine.any(Function));
          expect(joiner.makeAdjacencyMatrix.length).toEqual(0);
        });
        
        it('should offer a function to compute the initial degrees', function() {
          expect(joiner.makeInitialDegrees).toBeDefined();
          expect(joiner.makeInitialDegrees).toEqual(jasmine.any(Function));
          expect(joiner.makeInitialDegrees.length).toEqual(0);
        });
             
      });
      
      it('should be able to create an adjacency matrix', function() {
        helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4"]);
        edges.push(helper.createSimpleEdge(nodes, 0, 1));
        edges.push(helper.createSimpleEdge(nodes, 0, 3));
        edges.push(helper.createSimpleEdge(nodes, 1, 2));
        edges.push(helper.createSimpleEdge(nodes, 2, 1));
        edges.push(helper.createSimpleEdge(nodes, 3, 0));
        edges.push(helper.createSimpleEdge(nodes, 3, 1));
        edges.push(helper.createSimpleEdge(nodes, 3, 2));
        
        expect(joiner.makeAdjacencyMatrix()).toEqual({
          "0": ["1", "3"],
          "1": ["2"],
          "2": ["1"],
          "3": ["0", "1", "2"]
        });
        
      });
      
      describe('computation of degrees', function() {
        
        beforeEach(function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3"]);
          edges.push(helper.createSimpleEdge(nodes, 0, 1));
          edges.push(helper.createSimpleEdge(nodes, 0, 3));
          edges.push(helper.createSimpleEdge(nodes, 1, 2));
          edges.push(helper.createSimpleEdge(nodes, 2, 1));
          edges.push(helper.createSimpleEdge(nodes, 3, 0));
          edges.push(helper.createSimpleEdge(nodes, 3, 1));
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
        });
        
        it('should create the matrix if necessary', function() {
          spyOn(joiner, "makeAdjacencyMatrix").andCallThrough();
          joiner.makeInitialDegrees();
          expect(joiner.makeAdjacencyMatrix).wasCalled();
        });
      
        it('should not create the matrix if done before', function() {
          joiner.makeAdjacencyMatrix();
          spyOn(joiner, "makeAdjacencyMatrix").andCallThrough();
          joiner.makeInitialDegrees();
          expect(joiner.makeAdjacencyMatrix).wasNotCalled();
        });
      
        it('should be able to populate the initial degrees', function() {
          var m = edges.length,
            one = 1 / m,
            two = 2 / m,
            three = 3 / m;
          
          expect(joiner.makeInitialDegrees()).toEqual({
            "0": {
              in: one,
              out: two
            },
            "1": {
              in: three,
              out: one
            },
            "2": {
              in: two,
              out: one
            },
            "3": {
              in: one,
              out: three
            }
          });
        });
      });

      describe('computation of deltaQ', function() {
        
        var m, zero, one, two, three
        
        beforeEach(function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3"]);
          edges.push(helper.createSimpleEdge(nodes, 0, 1));
          edges.push(helper.createSimpleEdge(nodes, 0, 3));
          edges.push(helper.createSimpleEdge(nodes, 1, 2));
          edges.push(helper.createSimpleEdge(nodes, 2, 1));
          edges.push(helper.createSimpleEdge(nodes, 3, 0));
          edges.push(helper.createSimpleEdge(nodes, 3, 1));
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          
          m = edges.length;
          zero = {
            in: 1/m,
            out: 2/m
          };
          one = {
            in: 3/m,
            out: 1/m
          };
          two = {
            in: 2/m,
            out: 1/m
          };
          three = {
            in: 1/m,
            out: 3/m
          };
        });
        
        it('should create matrix if necessary', function() {
          spyOn(joiner, "makeAdjacencyMatrix").andCallThrough();
          joiner.makeInitialDQ();
          expect(joiner.makeAdjacencyMatrix).wasCalled();
        });
      
        it('should not create matrix if done before', function() {
          joiner.makeAdjacencyMatrix();
          spyOn(joiner, "makeAdjacencyMatrix").andCallThrough();
          joiner.makeInitialDQ();
          expect(joiner.makeAdjacencyMatrix).wasNotCalled();
        });
        
        it('should create degrees if necessary', function() {
          spyOn(joiner, "makeInitialDegrees").andCallThrough();
          joiner.makeInitialDQ();
          expect(joiner.makeInitialDegrees).wasCalled();
        });
      
        it('should not create degrees if done before', function() {
          joiner.makeInitialDegrees();
          spyOn(joiner, "makeInitialDegrees").andCallThrough();
          joiner.makeInitialDQ();
          expect(joiner.makeInitialDegrees).wasNotCalled();
        });
        
        it('should be able to populate the inital deltaQ', function() {
          expect(joiner.makeInitialDQ()).toEqual({
            "0": {
              "1": 1/m - zero.in * one.out,
              "3": 2/m - zero.in * three.out - zero.out * three.in
            },
            "1": {
              "2": 2/m - one.in * two.out - one.out * two.in,
              "3": 1/m - one.out * three.in
            },
            "2": {
              "3": 1/m - two.out * three.in
            },
          });
        });
        
      });

    });
  });
}());
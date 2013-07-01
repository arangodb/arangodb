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
      edges,
      testNetFour;

      beforeEach(function () {
        nodes = [];
        edges = [];
        joiner = new ModularityJoiner(nodes, edges);
        testNetFour = function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3"]);
          edges.push(helper.createSimpleEdge(nodes, 0, 1));
          edges.push(helper.createSimpleEdge(nodes, 0, 3));
          edges.push(helper.createSimpleEdge(nodes, 1, 2));
          edges.push(helper.createSimpleEdge(nodes, 2, 1));
          edges.push(helper.createSimpleEdge(nodes, 2, 3));
          edges.push(helper.createSimpleEdge(nodes, 3, 0));
          edges.push(helper.createSimpleEdge(nodes, 3, 1));
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
        };
      });
      
      describe('getters', function() {
        
        beforeEach(function() {
          this.addMatchers({
            toBeGetter: function() {
              var func = joiner[this.actual];
              if (!func) {
                this.message = function() {
                  return "Expected " + this.actual + " to be defined.";
                };
                return false;
              }
              if ("function" !== typeof func) {
                this.message = function() {
                  return "Expected " + this.actual + " to be a function.";
                };
                return false;
              }
              if (func.length !== 0) {
                this.message = function() {
                  return "Expected " + this.actual + " to be a getter function.";
                };
                return false;
              }
              return true;
            }
          });
        })
        
        it('should offer the adjacency matrix', function() {
          expect("getAdjacencyMatrix").toBeGetter();
        });
        
        it('should offer the heap', function() {
          expect("getHeap").toBeGetter();
        });
        
        it('should offer the delta qs', function() {
          expect("getDQ").toBeGetter();
        });
        
        it('should offer the degrees', function() {
          expect("getDegrees").toBeGetter();
        });
        
        it('should offer the best join', function() {
          expect("getBest").toBeGetter();
        });
        
        it('should offer the community list', function() {
          expect("getCommunities").toBeGetter();
        });
        
      });
      
      describe('checking the interface', function() {
        
        it('should offer a setup function', function() {
          expect(joiner.setup).toBeDefined();
          expect(joiner.setup).toEqual(jasmine.any(Function));
          expect(joiner.setup.length).toEqual(0);
        });
        
        it('should offer a function to join two communities', function() {
          expect(joiner.joinCommunity).toBeDefined();
          expect(joiner.joinCommunity).toEqual(jasmine.any(Function));
          expect(joiner.joinCommunity.length).toEqual(1);
        });
        
      });

      describe('after setup', function() {
        
        beforeEach(function() {
          testNetFour();
          joiner.setup();
        });
        
        describe('the adjacency matrix', function() {
          
          it('should be created', function() {
            expect(joiner.getAdjacencyMatrix()).toEqual({
              "0": ["1", "3"],
              "1": ["2"],
              "2": ["1", "3"],
              "3": ["0", "1", "2"]
            });
        
          });
        });
        
        describe('the degrees', function() {
        
          it('should initialy be populated', function() {
            var m = edges.length,
              one = 1 / m,
              two = 2 / m,
              three = 3 / m;
          
            expect(joiner.getDegrees()).toEqual({
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
                out: two
              },
              "3": {
                in: two,
                out: three
              }
            });
          });
        });
        
        describe('the deltaQ', function() {
        
          var m, zero, one, two, three
        
          beforeEach(function() {          
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
              out: 2/m
            };
            three = {
              in: 2/m,
              out: 3/m
            };
          });
        
          it('should initialy be populated', function() {
            expect(joiner.getDQ()).toEqual({
              "0": {
                "1": 1/m - zero.in * one.out,
                "3": 2/m - zero.in * three.out - zero.out * three.in
              },
              "1": {
                "2": 2/m - one.in * two.out - one.out * two.in,
                "3": 1/m - one.out * three.in
              },
              "2": {
                "3": 2/m - two.in * three.out - two.out * three.in
              },
            });
          });
        
        });
        
        describe('the heap', function() {
        
          var m, zero, one, two, three
        
          beforeEach(function() {          
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
              out: 2/m
            };
            three = {
              in: 2/m,
              out: 3/m
            };
          });

          it('should initialy by populated', function() {
            expect(joiner.getHeap()).toEqual({
              "0": "3",
              "1": "2",
              "2": "3"
            });
          });
        
          it('should return the largest value', function() {
            expect(joiner.getBest()).toEqual({
              sID: "0",
              lID: "3",
              val: 2/m - zero.in * three.out - zero.out * three.in
            });
          });
        
        });
        
        describe('communities', function() {
        
          it('should be able to get the communities', function() {
            // No communities yet. Should not return single nodes.
            expect(joiner.getCommunities()).toEqual({});
          });
        
          it('should be able to join two communities', function() {
            var toJoin = joiner.getBest(),
              joinVal = toJoin.val,
              m = edges.length,
              zero = {
                in: 1/m,
                out: 2/m
              },
              one = {
                in: 3/m,
                out: 1/m
              },
              two = {
                in: 2/m,
                out: 2/m
              },
              three = {
                in: 2/m,
                out: 3/m
              };
            expect(toJoin).toEqual({
              sID: "0",
              lID: "3",
              val: 2/m - zero.in * three.out - zero.out * three.in
            });
            joiner.joinCommunity(toJoin);
            expect(joiner.getCommunities()).toEqual({
              "0": {
                nodes: ["0", "3"],
                q: joinVal
              }
            });
          });
        
        });
        
      });
      
    });
  });
}());
/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global waitsFor, runs */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global GraphViewer*/

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


describe("Graph Viewer", function() {
  "use strict";
  var viewer,
  svg,
  docSVG,
  /*
  nodeWithID = function(id) {
    return $.grep(expander.nodes, function(e){
      return e.id === id;
    })[0];
  },
  
  allEdgesWithTargetID = function(id) {
    return $.grep(expander.edges, function(e){
      return e.target.id === id;
    });
  },
  
  allEdgesWithSourceID = function(id) {
    return $.grep(expander.edges, function(e){
      return e.source.id === id;
    });
  },
  
  edgeWithTargetID = function(id) {
    return allEdgesWithTargetID(id)[0];
  },
  
  edgeWithSourceID = function(id) {
    return allEdgesWithSourceID(id)[0];
  },
  
  existNode = function(id) {
    var node = nodeWithID(id);
    expect(node).toBeDefined();
    expect(node.id).toEqual(id);
  },
  
  notExistNode = function(id) {
    var node = nodeWithID(id);
    expect(node).toBeUndefined();
  },
  
  existNodes = function(ids) {
    _.each(ids, existNode);
  },
  
  notExistNodes = function(ids) {
    _.each(ids, notExistNode);
  },
  */
  displayNode = function(id) {
    expect($("svg #" + id)[0]).toBeDefined();
  },
  
  displayNodes = function(ids) {
    _.each(ids, displayNode);
  },
  
  notDisplayNode = function(id) {
    expect($("svg #" + id)[0]).toBeUndefined();
  },
  
  notDisplayNodes = function(ids) {
    _.each(ids, notDisplayNode);
  },
  
  clickOnNode = function(id) {
    var raw = $("#"+id).get(0);
    raw.__onclick();
  };

  /*
  var existEdge = function(source, target) {
    
  };
  
  var existEdges = function(stpairs) {
    _.each(stpairs, function(st) {
      existsEdge(st[0],st[1]);
    });
  };
  */
  
  
  beforeEach(function() {
    docSVG = document.createElement("svg");
    document.body.appendChild(docSVG);
    svg = d3.select("svg");
  });
  
  
  afterEach(function() {
    document.body.removeChild(docSVG);
  });
  
  describe('setup process', function() {
    
    var adapterConfig,
    layouterConfig;
    
    beforeEach(function() {
      adapterConfig = {type: "json", path: "../test_data"};
      layouterConfig = {type: "force"};
    });
    
    it('should throw an error if the svg is not given or incorrect', function() {
      expect(
        function() {
          var t = new GraphViewer();
        }
      ).toThrow("SVG has to be given and has to be selected using d3.select");
      expect(
        function() {
          var t = new GraphViewer(docSVG);
        }
      ).toThrow("SVG has to be given and has to be selected using d3.select");
    });
    
    it('should throw an error if the width is not given or incorrect', function() {
      expect(
        function() {
          var t = new GraphViewer(svg);
        }
      ).toThrow("A width greater 0 has to be given");
      expect(
        function() {
          var t = new GraphViewer(svg, -10);
        }
      ).toThrow("A width greater 0 has to be given");
    });
    
    it('should throw an error if the height is not given or incorrect', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10);
        }
      ).toThrow("A height greater 0 has to be given");
      expect(
        function() {
          var t = new GraphViewer(svg, 10, -10);
        }
      ).toThrow("A height greater 0 has to be given");
    });
    
    it('should throw an error if the adapter config is not given', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10, 10);
        }
      ).toThrow("Adapter config has to be given");
    });
    
    it('should throw an error if the node shaper config is not given', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10, 10, adapterConfig);
        }
      ).toThrow("Node Shaper config has to be given");
    });
    
    it('should throw an error if the edge shaper config is not given', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10, 10, adapterConfig, {});
        }
      ).toThrow("Edge Shaper config has to be given");
    });
    
    it('should throw an error if the layouter config is not given', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10, 10, adapterConfig, {}, {});
        }
      ).toThrow("Layouter config has to be given");
    });
    
    it('should throw an error if the events config is not given', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10, 10, adapterConfig, {}, {}, layouterConfig);
        }
      ).toThrow("Events config has to be given");
    });
    
    it('should not throw an error if everything is given', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10, 10, adapterConfig, {}, {}, layouterConfig, {});
        }
      ).not.toThrow();
    });
    
  });
  

  
/*  
  it("should be able to load a root node", function() {
    
    runs (function() {
      expander.loadGraph(477);
    });
    
    waitsFor(function() {
      return expander.nodes.length >= 5;
    }, "Some nodes should be added to the list", 1000);
    
    runs (function() {
      // Load 5 Nodes: Root + 4 Childs
      expect(expander.nodes.length).toBe(5);
      // Connect Root to all 4 Childs
      expect(expander.edges.length).toBe(4);
      // Pointer to correct Node Objects (d3 adds internal values => test creation not possible)
      
      existNodes([477, 29, 159, 213, 339]);
      
      // Pointer to correct Edge Objects (d3 adds internal values => test creation not possible)
      var e1 = edgeWithTargetID(29),
      e2 = edgeWithTargetID(159),
      e3 = edgeWithTargetID(213),
      e4 = edgeWithTargetID(339);
      // Check if the Edges have been added correctly
      expect(e1).toBeDefined();
      expect(e1.source.id).toEqual(477);
      expect(e2).toBeDefined();
      expect(e2.source.id).toEqual(477);
      expect(e3).toBeDefined();
      expect(e3.source.id).toEqual(477);
      expect(e4).toBeDefined();
      expect(e4.source.id).toEqual(477);
    });

  });
  
  it("and should append nodes to the SVG", function() {
  
    runs (function() {
      expander.loadGraph(477);
    });
    
    waitsFor(function() {
      return expander.nodes.length >= 5;
    }, "Some nodes should be added to the list", 1000);
    
    runs (function() {
      displayNodes([477, 339, 29, 159, 213]);
    });
  
  });
  
  describe("when graph has been loaded", function() {
    beforeEach(function() {
      
      runs (function() {
        expander.loadGraph(477);
      });
    
      waitsFor(function() {
        return expander.nodes.length > 0;
      }, "Some nodes should be added to the list", 1000);
      
    });
    
    it("should still be correct", function() {
      // Load 5 Nodes: Root + 4 Childs
      expect(expander.nodes.length).toBe(5);
      // Connect Root to all 4 Childs
      expect(expander.edges.length).toBe(4);
      
      existNodes([477, 29, 159, 213, 339]);
            
      // Pointer to correct Edge Objects (d3 adds internal values => test creation not possible)
      var e1 = edgeWithTargetID(29),
      e2 = edgeWithTargetID(159),
      e3 = edgeWithTargetID(213),
      e4 = edgeWithTargetID(339);
      // Check if the Edges have been added correctly
      expect(e1).toBeDefined();
      expect(e1.source.id).toEqual(477);
      expect(e2).toBeDefined();
      expect(e2.source.id).toEqual(477);
      expect(e3).toBeDefined();
      expect(e3.source.id).toEqual(477);
      expect(e4).toBeDefined();
      expect(e4.source.id).toEqual(477);
      
    });
    
    it("should be able to expand a node", function() {
    
      runs (function() {
        clickOnNode(29);
      });
    
      waitsFor(function() {
        return expander.nodes.length > 5;
      }, "More nodes should be added to the list", 1000);
    
      runs (function() {
        // Load 7 Nodes: Root + 4 Childs + 2 ChildChilds
        expect(expander.nodes.length).toBe(7);
        // Connect Root to all 4 Childs + Connect Child 29 to its Childs
        expect(expander.edges.length).toBe(6);
        
        existNodes([3, 7]);
      
        // Pointer to correct Edge Objects (d3 adds internal values => test creation not possible)
        var e1 = edgeWithTargetID(3),
        e2 = edgeWithTargetID(7);
      
        // Check if the Edges have been added correctly
        expect(e1).toBeDefined();
        expect(e1.source.id).toEqual(29);
        expect(e2).toBeDefined();
        expect(e2.source.id).toEqual(29);
      });

    });
    
    it("and should append all nodes to the SVG", function() {
    
      runs (function() {
        clickOnNode(29);
      });
    
      waitsFor(function() {
        return expander.nodes.length > 5;
      }, "More nodes should be added to the list", 1000);
    
      runs (function() {
        displayNodes([3, 7]);  
      });

    });
    
    it("should be able to collapse the root", function() {
    
      runs (function() {
        clickOnNode(477);
      });
    
      waitsFor(function() {
        return expander.nodes.length < 5;
      }, "Some nodes should be removed from the list", 1000);
    
      runs (function() {
        // Load 1 Nodes: Root
        expect(expander.nodes.length).toBe(1);
        // No edges expected
        expect(expander.edges.length).toBe(0);
        // Pointer to correct Node Objects (d3 adds internal values => test creation not possible)
        existNodes([477]);
      });

    });
    
    it("should be able to load a different graph", function() {
      runs (function() {
        expander.loadGraph(300);
      });
    
      waitsFor(function() {
        return expander.nodes.length !== 5
          && expander.nodes.length !== 0;
      }, "The list of nodes should be changed but not empty", 1000);
    
      runs (function() {
        // Load 3 Nodes: Root + 2 childs
        expect(expander.nodes.length).toBe(3);
        // No edges expected
        expect(expander.edges.length).toBe(2);
        
        // Pointer to correct Node Objects (d3 adds internal values => test creation not possible)
        existNodes([300, 29, 83]);
        
        // Pointer to correct Edge Objects (d3 adds internal values => test creation not possible)
        
        var e1 = edgeWithTargetID(29),
        e2 = edgeWithTargetID(83);
      
        // Check if the Edges have been added correctly
        expect(e1).toBeDefined();
        expect(e1.source.id).toBe(300);
        expect(e2).toBeDefined();
        expect(e2.source.id).toBe(300);
      });
      
    });
    
    it("and should replace all nodes in the SVG", function() {
      runs (function() {
        expander.loadGraph(300);
      });
    
      waitsFor(function() {
        return expander.nodes.length !== 5
          && expander.nodes.length !== 0;
      }, "The list of nodes should be changed but not empty", 1000);
    
      runs (function() {
        displayNodes([300, 29, 83]);       
      });
      
    });
    
    describe("when a user rapidly expand nodes", function() {
      
      beforeEach(function() {
        runs (function() {
          clickOnNode(29);
          clickOnNode(159);
          clickOnNode(213);
          clickOnNode(339);
        });
    
        waitsFor(function() {
          return expander.nodes.length === 14;
        }, "More nodes should be added to the list", 1000);
      });
      
      it("the graph should still be correct", function() {
        
        // Load 14 Nodes: Root + 4 Childs + 9 ChildChilds
        expect(expander.nodes.length).toBe(14);
        // Connect Root to all 4 Childs + Connect Child 29 to its Childs
        expect(expander.edges.length).toBe(13);

        existNodes([477, 213, 29, 3, 7, 4, 123, 80, 310, 339, 159, 84, 13, 20]);
      
      });
      
      it("and all expanded nodes should be displayed in the svg", function() {
        displayNodes([477, 213, 29, 3, 7, 4, 123, 80, 310, 339, 159, 84, 13, 20]);
      });
    });
    
    describe("when a user rapidly expand and collapses nodes", function() {
      
      beforeEach(function() {
        runs (function() {
          clickOnNode(29);
          clickOnNode(213);
          clickOnNode(339);
          clickOnNode(159);
        });
        
        // Wait a gentle second for all nodes to expand properly
        waitsFor(function() {
          return expander.nodes.length === 14;
        }, "More nodes should be added to the list", 1000);
        
        runs(function() {
          clickOnNode(29);
          clickOnNode(213);
        });
        
        waitsFor(function() {
          return expander.nodes.length === 10;
        }, "More nodes should be added to the list", 1000);
        
      });
      
      it("the graph should still be correct", function() {
        expect(expander.nodes.length).toBe(10);
        expect(expander.edges.length).toBe(9);

        existNodes([477, 213, 29, 339, 123, 310, 159, 84, 13, 20]);
      });
      
      it("and all expanded nodes should be displayed in the svg", function() {
        displayNodes([477, 213, 29, 339, 123, 310, 159, 84, 13, 20]);
      });
    });
    
    describe("when an undirected circle has been loaded", function() {
      beforeEach(function() {

        runs (function() {
          clickOnNode(29);
        });
        
        waitsFor(function() {
          return expander.nodes.length > 5;
        }, "Some nodes should be added to the list", 1000);
        
        runs (function() {
          clickOnNode(3);
        });
    
        waitsFor(function() {
          return expander.nodes.length > 7;
        }, "Some nodes should be added to the list", 1000);
      
        runs (function() {
          clickOnNode(213);
        });
    
        waitsFor(function() {
          return expander.nodes.length > 8;
        }, "Some nodes should be added to the list", 1000);
      
      });
      
      it("should still be correct", function() {
        // Load 5 Nodes: Root + 4 Childs
        expect(expander.nodes.length).toBe(9);
        // Connect Root to all 4 Childs
        expect(expander.edges.length).toBe(9);
        // Pointer to correct Node Objects (d3 adds internal values => test creation not possible)
        existNodes([477, 29, 159, 213, 339, 3, 7, 4, 80]);
        
      });
      
      it("and display all nodes in the SVG", function() {
        displayNodes([477, 29, 159, 213, 339, 3, 7, 4, 80]);
      });
      
      it("should contain an undirected circle", function() {
      
        var e1 = edgeWithTargetID(213),
        e2 = edgeWithTargetID(29),
        e3 = edgeWithTargetID(3),
        edgesTo4 = allEdgesWithTargetID(4);
        // Check if the Edges have been added correctly
        expect(e1.source.id).toBe(477);
        expect(e2.source.id).toBe(477);
        expect(e3.source.id).toBe(29);
        if (edgesTo4[0].source.id === 3) {
          expect(edgesTo4[1].source.id).toBe(213);
        } else {
          expect(edgesTo4[0].source.id).toBe(213);
          expect(edgesTo4[1].source.id).toBe(3);
        }

      });
    
      it("should be able to collapse one node "
       + "without removing the double referenced one", function() {
      
        runs (function() {
          clickOnNode(3);
        });
      
        waitsFor(function() {
          return expander.edges.length !== 9;
        }, "Edges should be changed", 1000);
      
        runs (function() {
          // There should be an edge from 213 -> 4, but none from 3
          var edgesFrom3 = allEdgesWithSourceID(3),
          edgesTo4 = allEdgesWithTargetID(4);
          expect(edgesFrom3.length).toEqual(0);
          expect(edgesTo4.length).toEqual(1);
          expect(edgesTo4[0].source.id).toEqual(213);
        });
      
      });
      
      it("and should display the nodes in the SVG", function() {
      
        runs (function() {
          clickOnNode(3);
        });
      
        waitsFor(function() {
          return expander.edges.length !== 9;
        }, "Edges should be changed", 1000);
      
        runs (function() {
          displayNodes([477, 29, 159, 213, 339, 3, 7, 4, 80]);
        });
      
      });
    
      it("should be able to collapse the other node "
       + "without removing the double referenced one", function() {
      
        runs (function() {
          clickOnNode(213);
        });
      
        waitsFor(function() {
          return expander.edges.length !== 9;
        }, "Edges should be changed", 1000);
      
        runs (function() {
          // There should be an edge from 3 -> 4, but none from 213
          var edgesFrom213 = allEdgesWithSourceID(213),
          edgesTo4 = allEdgesWithTargetID(4);
          expect(edgesFrom213.length).toEqual(0);
          expect(edgesTo4.length).toEqual(1);
          expect(edgesTo4[0].source.id).toEqual(3);
        
          // The orphan node should be removed
          notExistNode(80);
        });
      
      });
      
      it("and should display the nodes in the SVG", function() {
      
        runs (function() {
          clickOnNode(213);
        });
      
        waitsFor(function() {
          return expander.edges.length !== 9;
        }, "Edges should be changed", 1000);
      
        runs (function() {
          displayNodes([477, 29, 159, 213, 339, 3, 7, 4]);
          // The orphan should be removed
          notDisplayNode(80);
        });
      
      });
    
      it("should remove the double referenced node if both pointers are collapsed", function() {
      
        runs (function() {
          clickOnNode(3);
        });
      
        waitsFor(function() {
          return expander.edges.length !== 9;
        }, "Edges should be changed", 1000);
      
        runs (function() {
          clickOnNode(213);
        });
      
        waitsFor(function() {
          return expander.edges.length !== 8;
        }, "Edges should be changed", 1000);
      
        runs (function() {
          // There should be no edges from 3 or 213
          var edgesFrom213 = allEdgesWithSourceID(213),
          edgesFrom3 = allEdgesWithSourceID(3);
          expect(edgesFrom213.length).toEqual(0);
          expect(edgesFrom3.length).toEqual(0);
        
          // The orphan node should now be removed
          notExistNode(4);
        });
      
      });
      
      it("and should display the nodes in the SVG", function() {
      
        runs (function() {
          clickOnNode(3);
        });
      
        waitsFor(function() {
          return expander.edges.length !== 9;
        }, "Edges should be changed", 1000);
      
        runs (function() {
          clickOnNode(213);
        });
      
        waitsFor(function() {
          return expander.edges.length !== 8;
        }, "Edges should be changed", 1000);
      
        runs (function() {
          displayNodes([477, 29, 159, 213, 339, 3, 7]);
          
          // The orphans should be remove
          notDisplayNodes([4, 80]);
        });
      
      });
      
    });
    
    describe("when a subgraph has been loaded", function() {
      beforeEach(function() {
      
        runs(function() {
          clickOnNode(213);
        });
    
        waitsFor(function() {
          return expander.nodes.length > 5;
        }, "Some nodes should be added to the list", 1000);
      
        runs(function() {
          clickOnNode(80);
        });
    
        waitsFor(function() {
          return expander.nodes.length > 7;
        }, "Some nodes should be added to the list", 1000);
      
      });
      
      it("should contain the subgraph", function() {
        existNodes([213, 80, 54, 18, 76]);
                
        var r2sr = edgeWithTargetID(80),
        sr2sc1 = edgeWithTargetID(54),
        sr2sc2 = edgeWithTargetID(18),
        sr2sc3 = edgeWithTargetID(76);
        
        expect(r2sr.source.id).toEqual(213);
        expect(sr2sc1.source.id).toEqual(80);
        expect(sr2sc2.source.id).toEqual(80);
        expect(sr2sc3.source.id).toEqual(80);
      });
      
      it("and display the subgraph nodes in the SVG", function() {
        displayNodes([213, 80, 54, 18, 76]);
      });
      
      it("should be able to collapse the root and remove the subgraph", function() {
        
        runs(function() {
          clickOnNode(213);
        });
        
        waitsFor(function() {
          return expander.nodes.length !== 10;
        }, "The nodes should be changed", 1000);
        
        runs(function() {
          expect(expander.nodes.length).toEqual(5);
          
          existNode(213);
          notExistNodes([80, 54, 18, 76]);
                  
          var r2sr = edgeWithTargetID(80),
          sr2sc1 = edgeWithTargetID(54),
          sr2sc2 = edgeWithTargetID(18),
          sr2sc3 = edgeWithTargetID(76);
          
          expect(r2sr).not.toBeDefined();
          expect(sr2sc1).not.toBeDefined();
          expect(sr2sc2).not.toBeDefined();
          expect(sr2sc3).not.toBeDefined();
        });
      });
      
      it("and collapse the subgraph in the SVG", function() {
        
        runs(function() {
          clickOnNode(213);
        });
        
        waitsFor(function() {
          return expander.nodes.length !== 10;
        }, "The nodes should be changed", 1000);
        
        runs(function() {
          displayNode(213);
          notDisplayNodes([80, 54, 18, 76]);
        });
      });
      
    });
    
    describe("when a complex graph has been loaded", function() {
      beforeEach(function() {
        runs(function() {
          clickOnNode(29);
        });
    
        waitsFor(function() {
          return expander.nodes.length !== 5;
        }, "Some nodes should be added to the list", 1000);
      
        runs(function() {
          clickOnNode(3);
        });
    
        waitsFor(function() {
          return expander.nodes.length !== 7;
        }, "Some nodes should be added to the list", 1000);
        
        
        runs(function() {
          clickOnNode(213);
        });
    
        waitsFor(function() {
          return expander.nodes.length !== 8;
        }, "Some nodes should be added to the list", 1000);
        
        runs(function() {
          clickOnNode(80);
        });
    
        waitsFor(function() {
          return expander.nodes.length !== 9;
        }, "Some nodes should be added to the list", 1000);
      
      });
    
      it("should be able to collapse a node "
      + "referencing a node connected to a subgraph", function() {
          
        runs(function() {          
          clickOnNode(29);
        });
          
        waitsFor(function() {
          return expander.nodes.length !== 12;
        }, "The nodes should be changed", 1000);
          
        runs(function() {
          expect(expander.nodes.length).toEqual(10);
          expect(expander.edges.length).toEqual(9);
            
          // The nodes should be correct
          existNodes([477, 29, 4, 213, 339, 159, 80, 76, 18, 54]);
            
          // Subgraph Nodes should be removed
          notExistNodes([3, 7]);
            
          // The edges should be correct
          expect(edgeWithTargetID(54).source.id).toEqual(80);
          expect(edgeWithTargetID(18).source.id).toEqual(80);
          expect(edgeWithTargetID(76).source.id).toEqual(80);
          expect(edgeWithTargetID(80).source.id).toEqual(213);
          expect(edgeWithTargetID(4).source.id).toEqual(213);
          expect(edgeWithTargetID(159).source.id).toEqual(477);
          expect(edgeWithTargetID(339).source.id).toEqual(477);
          expect(edgeWithTargetID(29).source.id).toEqual(477);
            
          // Subgraph Edges should be removed
          expect(allEdgesWithTargetID(7).length).toEqual(0);
          expect(allEdgesWithTargetID(3).length).toEqual(0);
          expect(allEdgesWithSourceID(29).length).toEqual(0);
          var edgesTo4 = allEdgesWithTargetID(4);
          expect(edgesTo4.length).toEqual(1);
          expect(edgesTo4[0].source.id).toEqual(213);
            
        });
      });
      
      it("and display everything in the SVG", function() {
          
        runs(function() {
          clickOnNode(29);
        });
          
        waitsFor(function() {
          return expander.nodes.length !== 12;
        }, "The nodes should be changed", 1000);
          
        runs(function() {
          displayNodes([477, 29, 4, 213, 339, 159, 80, 76, 18, 54]);
          notDisplayNodes([3, 7]);
        });
      });      
    });
  });
  */
});
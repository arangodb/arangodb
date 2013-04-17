/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global EdgeShaper*/

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

  describe('Edge Shaper', function() {
    
    var svg;
    
    beforeEach(function () {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
    });

    afterEach(function () {
      //document.body.removeChild(svg);
    });
    
    
    it('should be able to draw an edge', function () {
      var source = {
        "_id": 1
      },
      target = {
        "_id": 2
      },
      edges = [
        {
          "source": source,
          "target": target
        }
      ],
      shaper = new EdgeShaper(d3.select("svg"));
      shaper.drawEdges(edges);
      expect($("svg .link line").length).toEqual(1);
      expect($("svg .link line")[0]).toBeDefined();
      expect($("svg #" + 1 + "-" + 2)[0]).toBeDefined();
    });

    it('should be able to draw many edges', function () {
      var one = {
        "_id": 1
      },
      two = {
        "_id": 2
      },
      three = {
        "_id": 3
      },
      four = {
        "_id": 4
      },
      edges = [
        {
          "source": one,
          "target": two
        },
        {
          "source": two,
          "target": three
        },
        {
          "source": three,
          "target": four
        },
        {
          "source": four,
          "target": one
        }
      ],
      shaper = new EdgeShaper(d3.select("svg"));
      shaper.drawEdges(edges);
      expect($("svg .link line").length).toEqual(4);
    });
    
    it('should be able to add an event', function () {
      var one = {
        "_id": 1
      },
      two = {
        "_id": 2
      },
      three = {
        "_id": 3
      },
      four = {
        "_id": 4
      },
      edges = [
        {
          "source": one,
          "target": two
        },
        {
          "source": two,
          "target": three
        },
        {
          "source": three,
          "target": four
        },
        {
          "source": four,
          "target": one
        }
      ],
      clicked = [],
      click = function (edge) {
        clicked[edge.source._id] = !clicked[edge.source._id];
      },
      shaper = new EdgeShaper(d3.select("svg"), {
        actions: {
          click: click
        }
      });
      shaper.drawEdges(edges);
      helper.simulateMouseEvent("click", "1-2");
      helper.simulateMouseEvent("click", "3-4");
      
      expect($("svg .link line").length).toEqual(4);
      expect(clicked[1]).toBeTruthy();
      expect(clicked[3]).toBeTruthy();
      expect(clicked[2]).toBeFalsy();
      expect(clicked[4]).toBeFalsy();
    });
    
    it('should be able to unbind all events', function() {
      var one = {
        "_id": 1
      },
      two = {
        "_id": 2
      },
      three = {
        "_id": 3
      },
      four = {
        "_id": 4
      },
      edges = [
        {
          "source": one,
          "target": two
        },
        {
          "source": two,
          "target": three
        },
        {
          "source": three,
          "target": four
        },
        {
          "source": four,
          "target": one
        }
      ],
      clicked = [],
      click = function (edge) {
        clicked[edge.source._id] = !clicked[edge.source._id];
      },
      shaper = new EdgeShaper(d3.select("svg"), {
        actions: {
          click: click
        }
      });
      shaper.drawEdges(edges);
      shaper.changeTo({
        actions: {
          reset: true
        }
      });
      
      helper.simulateMouseEvent("click", "1-2");
      helper.simulateMouseEvent("click", "3-4");
      
      expect($("svg .link line").length).toEqual(4);
      expect(clicked[1]).toBeFalsy();
      expect(clicked[3]).toBeFalsy();
      expect(clicked[2]).toBeFalsy();
      expect(clicked[4]).toBeFalsy();
    });
    
    it('should be able to add an arrow on target side', function() {
      var one = {
        "_id": 1
      },
      two = {
        "_id": 2
      },
      edges = [
        {
          "source": one,
          "target": two
        }
      ],
      shaper = new EdgeShaper(d3.select("svg"), {
        shape: {
          type: EdgeShaper.shapes.ARROW
        }
      });
      shaper.drawEdges(edges);
      
      expect($("#1-2 line").attr("marker-end")).toEqual("url(#arrow)");
      expect($("svg defs marker").length).toEqual(1);
      expect($("svg defs #arrow").length).toEqual(1);
      // Orientation is important. Other layout not yet.
      expect($("svg defs #arrow").attr("orient")).toEqual("auto");
    });
    
    it('should position the edges correctly', function() {
      var center = {
        _id: 1,
        x: 20,
        y: 20
      },
      NE = {
        _id: 2,
        x: 30,
        y: 10
      },
      SE = {
        _id: 3,
        x: 40,
        y: 30
      },
      SW = {
        _id: 4,
        x: 10,
        y: 40
      },
      NW = {
        _id: 5,
        x: 0,
        y: 0
      },
      edges = [
        {
          source: center,
          target: NE
        },
        {
          source: center,
          target: SE
        },
        {
          source: center,
          target: SW
        },
        {
          source: center,
          target: NW
        }
      ],
      shaper = new EdgeShaper(d3.select("svg"), {
        shape: {
          type: EdgeShaper.shapes.ARROW
        }
      });
      shaper.drawEdges(edges);
      
      //Check Position and rotation
      expect($("#1-2").attr("transform")).toEqual("translate(20, 20)rotate(-45)");
      expect($("#1-3").attr("transform")).toEqual("translate(20, 20)rotate(26.56505117707799)");
      expect($("#1-4").attr("transform")).toEqual("translate(20, 20)rotate(116.56505117707799)");
      expect($("#1-5").attr("transform")).toEqual("translate(20, 20)rotate(-135)");
      
      //Check length of line
      expect($("#1-2 line").attr("x2")).toEqual("14.142135623730951");
      expect($("#1-3 line").attr("x2")).toEqual("22.360679774997898");
      expect($("#1-4 line").attr("x2")).toEqual("22.360679774997898");
      expect($("#1-5 line").attr("x2")).toEqual("28.284271247461902");
    });
    
    describe('testing for colours', function() {
      
      it('should have a default colouring of no colour flag is given', function() {
        var nodes = [{_id: 1}, {_id: 2}],
        edges = [{
          source: nodes[0],
          target: nodes[1]
        },{
          source: nodes[1],
          target: nodes[0]
        }],
        shaper = new EdgeShaper(d3.select("svg"));
        shaper.drawEdges(edges);
        
        expect($("#1-2 line").attr("stroke")).toEqual("#686766");
        expect($("#2-1 line").attr("stroke")).toEqual("#686766");
      });
      
      it('should be able to use the same colour for all edges', function() {
        var s = {_id: 1},
        t = {_id: 2},
        edges = [{
          source: s,
          target: t
        },{
          source: t,
          target: s
        }],
        shaper = new EdgeShaper(d3.select("svg"),
          {
            color: {
              type: "single",
              stroke: "#123456"
            }
          }
        );
        shaper.drawEdges(edges);
        
        expect($("#1-2 line").attr("stroke")).toEqual("#123456");
        expect($("#2-1 line").attr("stroke")).toEqual("#123456");
      });
      
      it('should be able to use a colour based on attribute value', function() {
        var n1 = {_id: 1},
        n2 = {_id: 2},
        n3 = {_id: 3},
        n4 = {_id: 4},
        edges = [{
          source: n1,
          target: n2,
          _data: {
            label: "lbl1"
          }
        },{
          source: n2,
          target: n3,
          _data: {
            label: "lbl2"
          }
        },{
          source: n3,
          target: n4,
          _data: {
            label: "lbl3"
          }
        },{
          source: n4,
          target: n1,
          _data: {
            label: "lbl1"
          }
        }],
        shaper = new EdgeShaper(d3.select("svg"),
          {
            color: {
              type: "attribute",
              key: "label"
            }
          }
        ),
        c1,c2,c3,c4;
        shaper.drawEdges(edges);
        
        c1 = $("#1-2").attr("stroke");
        c2 = $("#2-3").attr("stroke");
        c3 = $("#3-4").attr("stroke");
        c4 = $("#4-1").attr("stroke");
        
        expect(c1).toBeDefined();
        expect(c2).toBeDefined();
        expect(c3).toBeDefined();
        expect(c4).toBeDefined();
        
        expect(c1).toEqual(c4);
        expect(c1).not.toEqual(c2);
        expect(c1).not.toEqual(c3);
        expect(c2).not.toEqual(c3);
        
        
      });
      
      it('should be able to use a gradient colour', function() {
        var s = {_id: 1},
        t = {_id: 2},
        edges = [{
          source: s,
          target: t
        }],
        shaper = new EdgeShaper(d3.select("svg"),
          {
            color: {
              type: "gradient",
              source: "#123456",
              target: "#654321"
            }
          }
        ),
        allStops;
        shaper.drawEdges(edges);
        
        expect($("#1-2 line").attr("stroke")).toEqual("url(#gradientEdgeColor)");
        
        // Sorry if the line is plain horizontal it is not displayed.
        expect($("#1-2 line").attr("y2")).toBeDefined();
        expect($("#1-2 line").attr("y2")).not.toEqual("0");
        
        // This check may not work
        //expect($("svg defs linearGradient").length).toEqual(1);
        expect($("svg defs #gradientEdgeColor").length).toEqual(1);
        
        allStops = $("svg defs #gradientEdgeColor stop");
        expect(allStops[0].getAttribute("offset")).toEqual("0");
        expect(allStops[1].getAttribute("offset")).toEqual("0.4");
        expect(allStops[2].getAttribute("offset")).toEqual("0.6");
        expect(allStops[3].getAttribute("offset")).toEqual("1");
        
        expect(allStops[0].getAttribute("stop-color")).toEqual("#123456");
        expect(allStops[1].getAttribute("stop-color")).toEqual("#123456");
        expect(allStops[2].getAttribute("stop-color")).toEqual("#654321");
        expect(allStops[3].getAttribute("stop-color")).toEqual("#654321");
        
      });
      
    });
    
    describe('configured for arrow shape', function() {
      var shaper;
      
      beforeEach(function() {
        var one = {
          "_id": 1
        },
        two = {
          "_id": 2
        },
        edges = [
          {
            "source": one,
            "target": two
          }
        ];
        shaper = new EdgeShaper(d3.select("svg"), {
          shape: {
            type: EdgeShaper.shapes.ARROW
          }
        });
        shaper.drawEdges(edges);
      });
      
      
      it('should clean the defs if a different shape is chosen', function() {
        shaper.changeTo({
          shape: {
            type: EdgeShaper.shapes.NONE
          }
        });
        expect($("#1-2").attr("marker-end")).toBeUndefined();
        expect($("svg defs marker#arrow").length).toEqual(0);
      });
      
      it('should not append the same marker multiple times', function() {
        shaper.changeTo({
          shape: {
            type: EdgeShaper.shapes.ARROW
          }
        });
        expect($("svg defs marker#arrow").length).toEqual(1);
        shaper.changeTo({
          shape: {
            type: EdgeShaper.shapes.ARROW
          }
        });
        expect($("svg defs marker#arrow").length).toEqual(1);
      });
    });
    
    describe('when edges are already drawn', function() {
      var edges,
      nodes,
      clicked,
      click = function (edge) {
        clicked[edge._id] = !clicked[edge._id];
      },
      shaper;
      
      beforeEach(function() {
        nodes = [{_id: 1}, {_id: 2}, {_id: 3}, {_id: 4}];
        edges = [
          {_id: 1, source: nodes[0], target: nodes[1]},
          {_id: 2, source: nodes[1], target: nodes[2]},
          {_id: 3, source: nodes[2], target: nodes[3]}
        ];
        clicked = [];
        shaper = new EdgeShaper(d3.select("svg"));
        shaper.drawEdges(edges);
        expect($("svg .link").length).toEqual(3);
      });
      
      it('should be able to add a click event to existing edges', function() {
        expect($("svg .link").length).toEqual(3);
        shaper.changeTo({
          actions: {
            click: click
          }
        });
        helper.simulateMouseEvent("click", "1-2");
        helper.simulateMouseEvent("click", "3-4");
        expect(clicked[1]).toBeTruthy();
        expect(clicked[3]).toBeTruthy();
        expect(clicked[2]).toBeUndefined();
      });
      
      it('should add a click event to newly arriving edges', function() {
        
        shaper.changeTo({
          actions: {
            click: click
          }
        });
        edges.push({_id: 4, source: nodes[3], target: nodes[0]});
        edges.push({_id: 5, source: nodes[0], target: nodes[2]});
        shaper.drawEdges(edges);
        
        helper.simulateMouseEvent("click", "4-1");
        helper.simulateMouseEvent("click", "1-3");
        expect($("svg .link").length).toEqual(5);
        expect(clicked[4]).toBeTruthy();
        expect(clicked[5]).toBeTruthy();
        expect(clicked[1]).toBeUndefined();
        expect(clicked[2]).toBeUndefined();
        expect(clicked[3]).toBeUndefined();
      });
      
      it('should display each edge exactly once if an event is added', function() {
        shaper.changeTo({
          actions: {
            click: function() {return 0;}
          }
        });
        expect($("svg .link").length).toEqual(3);
        shaper.changeTo({
          actions: {
            click: function() {return 0;}
          }
        });
        expect($("svg .link").length).toEqual(3);
        shaper.changeTo({
          actions: {
            click: function() {return 0;}
          }
        });
        expect($("svg .link").length).toEqual(3);
      });
    });
    
    describe('configured for label', function() {
      
      var shaper;
      
      beforeEach(function() {
        shaper = new EdgeShaper(d3.select("svg"), {"label": "label"});
      });
      
      it('should display the correct label', function() {
        var nodes = [{"_id": 1}, {"_id": 2}, {"_id": 3}, {"_id": 4}],
        edges = [
          {
            "source": nodes[0],
            "target": nodes[1],
            _data: {
              label: "first"
            }
          },
          {
            "source": nodes[1],
            "target": nodes[2],
            _data: {
              label: "second"
            }
          },
          {
            "source": nodes[2],
            "target": nodes[3],
            _data: {
              label: "third"
            }
          },
          {
            "source": nodes[3],
            "target": nodes[0],
            _data: {
              label: "fourth"
            }
          }
        ];
        shaper.drawEdges(edges);
        
        expect($("text").length).toEqual(4);
        expect($("#1-2 text")[0].textContent).toEqual("first");
        expect($("#2-3 text")[0].textContent).toEqual("second");
        expect($("#3-4 text")[0].textContent).toEqual("third");
        expect($("#4-1 text")[0].textContent).toEqual("fourth");
        
      });
      
      it('should display the label at the correct position', function() {
        var nodes = [
          {
            _id: 1,
            x: 20,
            y: 20
          },
          {
            _id: 2,
            x: 100,
            y: 20
          }
        ],
        edges = [
          {
            "source": nodes[0],
            "target": nodes[1],
            _data: {
              "label": "first"
            }
          }
        ];
        shaper.drawEdges(edges);
        
        expect($("#1-2 text").attr("transform")).toEqual("translate(40, -3)");
      });
      
      it('should ignore other attributes', function() {
        var nodes = [{"_id": 1}, {"_id": 2}, {"_id": 3}, {"_id": 4}],
        edges = [
          {
            "source": nodes[0],
            "target": nodes[1],
            _data: {
              "label": "correct"
            }
          },
          {
            "source": nodes[1],
            "target": nodes[2],
            _data: {
              "alt": "incorrect"
            }
          },
          {
            "source": nodes[2],
            "target": nodes[3],
            _data: {}
          },
          {
            "source": nodes[3],
            "target": nodes[0],
            _data: {
              "label": "correct",
              "alt": "incorrect"
            }
          }
        ];
        shaper.drawEdges(edges);
        
        expect($("text").length).toEqual(4);
        expect($("#1-2 text")[0].textContent).toEqual("correct");
        expect($("#2-3 text")[0].textContent).toEqual("");
        expect($("#3-4 text")[0].textContent).toEqual("");
        expect($("#4-1 text")[0].textContent).toEqual("correct");
      });
      
      it('should be able to switch to another label', function() {
        var nodes = [{"_id": 1}, {"_id": 2}],
        edges = [
          {
            "source": nodes[0],
            "target": nodes[1],
            _data: {
              "label": "old"
            }
          },
          {
            "source": nodes[1],
            "target": nodes[0],
            _data: {
              "new": "new"
            }
          }
        ];
        shaper.drawEdges(edges);
        
        expect($("#1-2")[0].textContent).toEqual("old");
        expect($("#2-1")[0].textContent).toEqual("");
        
        shaper.changeTo({label: "new"});
        
        expect($("#1-2")[0].textContent).toEqual("");
        expect($("#2-1")[0].textContent).toEqual("new");
      });
      
    });
    
    describe('using a function for labels', function() {
      
      var shaper,
      labelFunction = function (edge) {
        if (edge.source._id === 1) {
          return "from first";
        }
        if (edge.target._id === 3) {
          return "to third";
        }
        return "default";
      };
      
      beforeEach(function() {
        shaper = new EdgeShaper(d3.select("svg"), {"label": labelFunction});
      });
      
      it('should display the correct label', function() {
        var nodes = [{"_id": 1}, {"_id": 2}, {"_id": 3}, {"_id": 4}],
        edges = [
          {
            "source": nodes[0],
            "target": nodes[1]
          },
          {
            "source": nodes[1],
            "target": nodes[2]
          },
          {
            "source": nodes[2],
            "target": nodes[3]
          },
          {
            "source": nodes[3],
            "target": nodes[0]
          }
        ];
        shaper.drawEdges(edges);
        
        expect($("text").length).toEqual(4);
        expect($("#1-2 text")[0].textContent).toEqual("from first");
        expect($("#2-3 text")[0].textContent).toEqual("to third");
        expect($("#3-4 text")[0].textContent).toEqual("default");
        expect($("#4-1 text")[0].textContent).toEqual("default");
      });
      
    });
    
    describe('that has already drawn some edges', function () {

      var shaper, edges, nodes;

      beforeEach(function () {
        shaper = new EdgeShaper(d3.select("svg"));
        nodes = [{"_id": 1}, {"_id": 2}, {"_id": 3}, {"_id": 4}];
        edges = [
          {
            "source": nodes[0],
            "target": nodes[1]
          },
          {
            "source": nodes[1],
            "target": nodes[2]
          },
          {
            "source": nodes[2],
            "target": nodes[3]
          },
          {
            "source": nodes[3],
            "target": nodes[0]
          }
        ];
        shaper.drawEdges(edges);
      });

      it('should be able to draw more edges', function () {
        edges.push(
          {
            "source": nodes[2],
            "target": nodes[0]
          }
        );
        edges.push({
            "source": nodes[1],
            "target": nodes[3]
          });
        shaper.drawEdges(edges);
        expect($("svg .link").length).toEqual(6);
        expect($("svg #2-3")[0]).toBeDefined();
        expect($("svg #4-1")[0]).toBeDefined();
        expect($("svg #3-1")[0]).toBeDefined();
        expect($("svg #2-4")[0]).toBeDefined();
        expect($("svg #1-2")[0]).toBeDefined();
        expect($("svg #3-4")[0]).toBeDefined();
        
      });

      it('should be able to remove edges', function () {
        edges.splice(2, 1);
        edges.splice(0, 1);
        shaper.drawEdges(edges);
        expect($("svg .link").length).toEqual(2);
        expect($("svg #2-3")[0]).toBeDefined();
        expect($("svg #4-1")[0]).toBeDefined();
        expect($("svg #1-2")[0]).toBeUndefined();
        expect($("svg #3-4")[0]).toBeUndefined();
      });

      it('should be able to add some edges and remove other edges', function () {
        edges.splice(2, 1);
        edges.splice(0, 1);
        edges.push(
          {
            "source": nodes[2],
            "target": nodes[0]
          }
        );
        edges.push({
            "source": nodes[1],
            "target": nodes[3]
        });
        shaper.drawEdges(edges);
        
        expect($("svg .link").length).toEqual(4);
        expect($("svg #2-3")[0]).toBeDefined();
        expect($("svg #4-1")[0]).toBeDefined();
        expect($("svg #3-1")[0]).toBeDefined();
        expect($("svg #2-4")[0]).toBeDefined();
        expect($("svg #1-2")[0]).toBeUndefined();
        expect($("svg #3-4")[0]).toBeUndefined();
        
      });

    });
    
    describe('that has an other parent then body.svg', function () {

      it('should draw edges in the correct position', function () {
        var internalObject = d3.select("svg")
          .append("g")
          .append("svg")
          .append("g"),
        shaper = new EdgeShaper(internalObject),
        source = {
          "_id": 1
        },
        target = {
          "_id": 2
        },
        edges = [
          {
            "source": source,
            "target": target
          }
        ];

        shaper.drawEdges(edges);

        expect($("svg g svg g .link line").length).toEqual(1);
      });
    });
    
  });

}());

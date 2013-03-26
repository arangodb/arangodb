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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
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
      document.body.removeChild(svg);
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
      expect($("svg line.link").length).toEqual(1);
      expect($("svg line.link")[0]).toBeDefined();
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
      expect($("svg line.link").length).toEqual(4);
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
      shaper = new EdgeShaper(d3.select("svg"));
      
      shaper.on("click", click);
      shaper.drawEdges(edges);
      helper.simulateMouseEvent("click", "1-2");
      helper.simulateMouseEvent("click", "3-4");
      
      expect($("svg line").length).toEqual(4);
      expect(clicked[1]).toBeTruthy();
      expect(clicked[3]).toBeTruthy();
      expect(clicked[2]).toBeFalsy();
      expect(clicked[4]).toBeFalsy();
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
        shaper.on("click", click);
        helper.simulateMouseEvent("click", "1-2");
        helper.simulateMouseEvent("click", "3-4");
        expect(clicked[1]).toBeTruthy();
        expect(clicked[3]).toBeTruthy();
        expect(clicked[2]).toBeUndefined();
      });
      
      it('should add a click event to newly arriving edges', function() {
        
        shaper.on("click", click);
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
        shaper.on("click", function() {return 0;});
        expect($("svg .link").length).toEqual(3);
        shaper.on("click", function() {return 1;});
        expect($("svg .link").length).toEqual(3);
        shaper.on("click", function() {return 2;});
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
            "label": "first"
          },
          {
            "source": nodes[1],
            "target": nodes[2],
            "label": "second"
          },
          {
            "source": nodes[2],
            "target": nodes[3],
            "label": "third"
          },
          {
            "source": nodes[3],
            "target": nodes[0],
            "label": "fourth"
          }
        ];
        shaper.drawEdges(edges);
        
        expect($("text").length).toEqual(4);
        expect($("#1-2 text")[0].textContent).toEqual("first");
        expect($("#2-3 text")[0].textContent).toEqual("second");
        expect($("#3-4 text")[0].textContent).toEqual("third");
        expect($("#4-1 text")[0].textContent).toEqual("fourth");
      });
      
      it('should ignore other attributes', function() {
        var nodes = [{"_id": 1}, {"_id": 2}, {"_id": 3}, {"_id": 4}],
        edges = [
          {
            "source": nodes[0],
            "target": nodes[1],
            "label": "correct"
          },
          {
            "source": nodes[1],
            "target": nodes[2],
            "alt": "incorrect"
          },
          {
            "source": nodes[2],
            "target": nodes[3]
          },
          {
            "source": nodes[3],
            "target": nodes[0],
            "label": "correct",
            "alt": "incorrect"
          }
        ];
        shaper.drawEdges(edges);
        
        expect($("text").length).toEqual(4);
        expect($("#1-2 text")[0].textContent).toEqual("correct");
        expect($("#2-3 text")[0].textContent).toEqual("");
        expect($("#3-4 text")[0].textContent).toEqual("");
        expect($("#4-1 text")[0].textContent).toEqual("correct");
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

      it('should be able to add some edges and remove other egdes', function () {
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

        expect($("svg g svg g line.link").length).toEqual(1);
      });
    });
    
  });

}());

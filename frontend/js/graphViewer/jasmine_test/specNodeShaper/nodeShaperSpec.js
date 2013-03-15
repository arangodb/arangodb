/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global NodeShaper*/

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

  describe('Node Shaper', function () {
    var svg;

    beforeEach(function () {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
    });

    afterEach(function () {
      document.body.removeChild(svg);
    });

    it('should be able to draw a node', function () {
      var node = [{"id": 1}],
        shaper = new NodeShaper(d3.select("svg"));
      shaper.drawNodes(node);
      expect($("svg .node").length).toEqual(1);
      expect($("svg .node")[0]).toBeDefined();
    });

    it('should be able to draw many nodes', function () {
      var nodes = [{"id": 1}, {"id": 2}, {"id": 3}],
        shaper = new NodeShaper(d3.select("svg"));
      shaper.drawNodes(nodes);
      expect($("svg .node").length).toEqual(3);
    });

    it('should be able to add a click event', function () {
      var nodes = [{"id": 1}, {"id": 2}, {"id": 3}],
      clicked = [false, false, false],
      click = function (node) {
        clicked[node.id-1] = !clicked[node.id-1];
      },
      shaper = new NodeShaper(d3.select("svg")),
      first,
      third;
      
      shaper.on("click", click);
      shaper.drawNodes(nodes);
      first = $("#1").get(0);
      third = $("#3").get(0);
      first.__onclick();
      third.__onclick();
      
      expect($("svg .node").length).toEqual(3);
      expect(clicked[0]).toBeTruthy();
      expect(clicked[2]).toBeTruthy();
      expect(clicked[1]).toBeFalsy();
    });

    describe('configured for circle', function () {
      var shaper;

      beforeEach(function () {
        shaper = new NodeShaper(d3.select("svg"), {"shape": NodeShaper.shapes.CIRCLE});
      });

      it('should draw circle elements', function () {
        var node = [{"id": 1}];
        shaper.drawNodes(node);
        expect($("svg .node")[0]).toBeDefined();
        expect($("svg .node").length).toEqual(1);
        expect($("svg #1")[0]).toBeDefined();
        expect($("svg circle")[0]).toBeDefined();
        expect($("svg circle").length).toEqual(1);
        expect($("svg .node circle")[0]).toBeDefined();
        expect($("svg .node circle").length).toEqual(1);
      });
      
      it('should be able to use a fixed radius', function() {
        shaper = new NodeShaper(d3.select("svg"),
        {
          "shape": NodeShaper.shapes.CIRCLE,
          "size": 15
        });
        var nodes = [
          {"id": 1},
          {"id": 2},
          {"id": 3},
          {"id": 4},
          {"id": 5}
        ];
        shaper.drawNodes(nodes);
        expect($("svg #1 circle")[0].attributes.r.value).toEqual("15");
        expect($("svg #2 circle")[0].attributes.r.value).toEqual("15");
        expect($("svg #3 circle")[0].attributes.r.value).toEqual("15");
        expect($("svg #4 circle")[0].attributes.r.value).toEqual("15");
        expect($("svg #5 circle")[0].attributes.r.value).toEqual("15");
      });
      
      it('should be able to use a function to determine the radius', function() {
        var radiusFunction = function (node) {
          return 10 + node.id;
        },
        nodes = [
          {"id": 1},
          {"id": 2},
          {"id": 3},
          {"id": 4},
          {"id": 5}
        ];
        shaper = new NodeShaper(d3.select("svg"),
        {
          "shape": NodeShaper.shapes.CIRCLE,
          "size": radiusFunction
        });
        shaper.drawNodes(nodes);
        expect($("svg #1 circle")[0].attributes.r.value).toEqual("11");
        expect($("svg #2 circle")[0].attributes.r.value).toEqual("12");
        expect($("svg #3 circle")[0].attributes.r.value).toEqual("13");
        expect($("svg #4 circle")[0].attributes.r.value).toEqual("14");
        expect($("svg #5 circle")[0].attributes.r.value).toEqual("15");
        
      });
    
    });
    
    describe('configured for rectangle', function () {
      var shaper;

      beforeEach(function () {
        shaper = new NodeShaper(d3.select("svg"), {"shape": NodeShaper.shapes.RECT});
      });

      it('should draw rect elements', function () {
        var node = [{"id": 1}];
        shaper.drawNodes(node);
        expect($("svg .node")[0]).toBeDefined();
        expect($("svg .node").length).toEqual(1);
        expect($("svg #1")[0]).toBeDefined();
        expect($("svg rect")[0]).toBeDefined();
        expect($("svg rect").length).toEqual(1);
        expect($("svg .node rect")[0]).toBeDefined();
        expect($("svg .node rect").length).toEqual(1);
      });
      
      it('should be able to use a fixed size', function() {
        shaper = new NodeShaper(d3.select("svg"),
        {
          "shape": NodeShaper.shapes.RECT,
          "width": 15,
          "height": 10
        });
        var nodes = [
          {"id": 1},
          {"id": 2},
          {"id": 3},
          {"id": 4},
          {"id": 5}
        ];
        shaper.drawNodes(nodes);
        expect($("svg #1 rect")[0].attributes.width.value).toEqual("15");
        expect($("svg #2 rect")[0].attributes.width.value).toEqual("15");
        expect($("svg #3 rect")[0].attributes.width.value).toEqual("15");
        expect($("svg #4 rect")[0].attributes.width.value).toEqual("15");
        expect($("svg #5 rect")[0].attributes.width.value).toEqual("15");
        
        expect($("svg #1 rect")[0].attributes.height.value).toEqual("10");
        expect($("svg #2 rect")[0].attributes.height.value).toEqual("10");
        expect($("svg #3 rect")[0].attributes.height.value).toEqual("10");
        expect($("svg #4 rect")[0].attributes.height.value).toEqual("10");
        expect($("svg #5 rect")[0].attributes.height.value).toEqual("10");
        
      });
      
      it('should be able to use a function to determine the size', function() {
        var widthFunction = function (node) {
          return 10 + node.id;
        },
        heightFunction = function (node) {
          return 10 - node.id;
        },
        nodes = [
          {"id": 1},
          {"id": 2},
          {"id": 3},
          {"id": 4},
          {"id": 5}
        ];
        shaper = new NodeShaper(d3.select("svg"),
        {
          "shape": NodeShaper.shapes.RECT,
          "width": widthFunction,
          "height": heightFunction
        });
        shaper.drawNodes(nodes);
        expect($("svg #1 rect")[0].attributes.width.value).toEqual("11");
        expect($("svg #2 rect")[0].attributes.width.value).toEqual("12");
        expect($("svg #3 rect")[0].attributes.width.value).toEqual("13");
        expect($("svg #4 rect")[0].attributes.width.value).toEqual("14");
        expect($("svg #5 rect")[0].attributes.width.value).toEqual("15");
        
        expect($("svg #1 rect")[0].attributes.height.value).toEqual("9");
        expect($("svg #2 rect")[0].attributes.height.value).toEqual("8");
        expect($("svg #3 rect")[0].attributes.height.value).toEqual("7");
        expect($("svg #4 rect")[0].attributes.height.value).toEqual("6");
        expect($("svg #5 rect")[0].attributes.height.value).toEqual("5");
        
        
      });
    
    });
    
    
    
    describe('configured for label', function () {
      var shaper;

      beforeEach(function () {
        shaper = new NodeShaper(d3.select("svg"), {
          "label": "label",
          "shape": NodeShaper.shapes.CIRCLE
        });
      });

      it('should add a text element', function () {
        var node = [{"id": 1, "label": "MyLabel"}];
        shaper.drawNodes(node);
        expect($("svg .node")[0]).toBeDefined();
        expect($("svg .node").length).toEqual(1);
        expect($("svg #1")[0]).toBeDefined();
        expect($("svg text")[0]).toBeDefined();
        expect($("svg text").length).toEqual(1);
        expect($("svg .node text")[0]).toBeDefined();
        expect($("svg .node text").length).toEqual(1);
        expect($("svg .node text")[0].textContent).toEqual("MyLabel");
      });

      it('should ignore other attributes', function () {
        var nodes = [
          {"id": 1, "label": "correct"},
          {"id": 2, "alt": "incorrect"},
          {"id": 3, "alt": "incorrect"},
          {"id": 4, "label": "correct", "alt": "incorrect"}];
        shaper.drawNodes(nodes);
        expect($("svg #1 text")[0].textContent).toEqual("correct");
        expect($("svg #2 text")[0].textContent).toEqual("");
        expect($("svg #3 text")[0].textContent).toEqual("");
        expect($("svg #4 text")[0].textContent).toEqual("correct");
      });

      it('should also print "0" as a label', function() {
        var node = [{"id": 1, "label": 0}];
        shaper.drawNodes(node);
        expect($("svg .node text")[0]).toBeDefined();
        expect($("svg .node text").length).toEqual(1);
        expect($("svg .node text")[0].textContent).toEqual("0");
      });

    });

    describe('using a function for labels', function () {
      var shaper,
      labelFunction = function (node) {
        if (node.id === 4) {
          return "correct";
        }
        if (node.label) {
          return node.label;
        }
        if (node.alt) {
          return node.alt;
        }
        return "default";
      };

      beforeEach(function () {
        shaper = new NodeShaper(d3.select("svg"), {
          "label": labelFunction,
          "shape": NodeShaper.shapes.CIRCLE
        });
      });

      it('should display the correct labels according to the function', function () {
        var nodes = [
          {"id": 1, "label": "correct"},
          {"id": 2, "alt": "correct"},
          {"id": 3, "label": "correct", "alt": "incorrect"},
          {"id": 4, "label": "incorrect", "alt": "incorrect"},
          {"id": 5}
        ];
        shaper.drawNodes(nodes);
        expect($("text").length).toEqual(5);
        expect($("#1 text")[0].textContent).toEqual("correct");
        expect($("#2 text")[0].textContent).toEqual("correct");
        expect($("#3 text")[0].textContent).toEqual("correct");
        expect($("#4 text")[0].textContent).toEqual("correct");
        expect($("#5 text")[0].textContent).toEqual("default");
      });

    });

    describe('that has already drawn some nodes', function () {

      var shaper, nodes;

      beforeEach(function () {
        shaper = new NodeShaper(d3.select("svg"));
        nodes = [{"id": 1}, {"id": 2}, {"id": 3}, {"id": 4}, {"id": 5}];
        shaper.drawNodes(nodes);
      });

      it('should be able to draw more nodes', function () {
        nodes.push({"id": 6});
        nodes.push({"id": 7});
        nodes.push({"id": 8});

        shaper.drawNodes(nodes);
        expect($("svg .node").length).toEqual(8);
      });

      it('should be able to remove nodes', function () {
        nodes.splice(1, 1);
        nodes.splice(3, 1);
        shaper.drawNodes(nodes);
        expect($("svg .node").length).toEqual(3);
        expect($("svg #1")[0]).toBeDefined();
        expect($("svg #3")[0]).toBeDefined();
        expect($("svg #4")[0]).toBeDefined();

        expect($("svg #2")[0]).toBeUndefined();
        expect($("svg #5")[0]).toBeUndefined();
      });

      it('should be able to add some nodes and remove other nodes', function () {
        nodes.splice(1, 1);
        nodes.splice(3, 1);
        nodes.push({"id": 6});
        nodes.push({"id": 7});
        nodes.push({"id": 8});
        shaper.drawNodes(nodes);
        expect($("svg .node").length).toEqual(6);
        expect($("svg #1")[0]).toBeDefined();
        expect($("svg #3")[0]).toBeDefined();
        expect($("svg #4")[0]).toBeDefined();
        expect($("svg #6")[0]).toBeDefined();
        expect($("svg #7")[0]).toBeDefined();
        expect($("svg #8")[0]).toBeDefined();

        expect($("svg #2")[0]).toBeUndefined();
        expect($("svg #5")[0]).toBeUndefined();
      });

    });

    describe('that has an other parent then body.svg', function () {

      it('should draw nodes in the correct position', function () {
        var internalObject = d3.select("svg")
          .append("g")
          .append("svg")
          .append("g"),
          node = [{"id": 1}],
          shaper = new NodeShaper(internalObject);

        shaper.drawNodes(node);

        expect($("svg g svg g .node").length).toEqual(1);
      });
    });

    describe('configured for circle with label', function () {
      var shaper;

      beforeEach(function () {
        shaper = new NodeShaper(d3.select("svg"),
            {
              "shape": NodeShaper.shapes.CIRCLE,
              "label": "label"
            }
          );
      });

      it('should draw circle elements', function () {
        var node = [{
          "id": 1,
          "label": "correct"
        }];
        shaper.drawNodes(node);
        expect($("svg .node").length).toEqual(1);
        expect($("svg #1")[0]).toBeDefined();
        expect($("svg circle")[0]).toBeDefined();
        expect($("svg circle").length).toEqual(1);
        expect($("svg .node circle")[0]).toBeDefined();
        expect($("svg .node circle").length).toEqual(1);
        
        expect($("svg text").length).toEqual(1);
        expect($("svg .node text").length).toEqual(1);
        expect($("svg .node text")[0].textContent).toEqual("correct");
      });

    });

  });

}());
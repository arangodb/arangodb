/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
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
      var node = [{_id: 1}],
        shaper = new NodeShaper(d3.select("svg"));
      shaper.drawNodes(node);
      expect($("svg .node").length).toEqual(1);
      expect($("svg .node")[0]).toBeDefined();
    });

    it('should be able to draw many nodes', function () {
      var nodes = [{_id: 1}, {_id: 2}, {_id: 3}],
        shaper = new NodeShaper(d3.select("svg"));
      shaper.drawNodes(nodes);
      expect($("svg .node").length).toEqual(3);
    });

    it('should be able to add an event', function () {
      var nodes = [{_id: 1}, {_id: 2}, {_id: 3}],
      clicked = [],
      click = function (node) {
        clicked[node._id] = !clicked[node._id];
      },
      shaper = new NodeShaper(d3.select("svg"),
      {
        actions: {
          click: click
        }
      });
      
      shaper.drawNodes(nodes);
      helper.simulateMouseEvent("click", "1");
      helper.simulateMouseEvent("click", "3");
      
      expect($("svg .node").length).toEqual(3);
      expect(clicked[1]).toBeTruthy();
      expect(clicked[3]).toBeTruthy();
      expect(clicked[2]).toBeUndefined();
    });
    
    it('should be able to unbind all events', function () {
      var nodes = [{_id: 1}, {_id: 2}, {_id: 3}],
      clicked = [],
      click = function (node) {
        clicked[node._id] = !clicked[node._id];
      },
      shaper = new NodeShaper(d3.select("svg"),
      {
        actions: {
          click: click
        }
      });
      
      shaper.drawNodes(nodes);
      
      shaper.changeTo({
        actions: {
          reset: true
        }
      });
      
      helper.simulateMouseEvent("click", "1");
      helper.simulateMouseEvent("click", "3");
      
      expect($("svg .node").length).toEqual(3);
      expect(clicked[1]).toBeUndefined();
      expect(clicked[3]).toBeUndefined();
      expect(clicked[2]).toBeUndefined();
    });

    it('should display circles by default', function() {
      var node = [{_id: 1}],
        shaper = new NodeShaper(d3.select("svg"));
      shaper.drawNodes(node);
      expect($("svg .node")[0]).toBeDefined();
      expect($("svg .node").length).toEqual(1);
      expect($("svg #1")[0]).toBeDefined();
      expect($("svg circle")[0]).toBeDefined();
      expect($("svg circle").length).toEqual(1);
      expect($("svg .node circle")[0]).toBeDefined();
      expect($("svg .node circle").length).toEqual(1);
      expect($("svg #1 circle")[0].attributes.r.value).toEqual("8");
    });

    describe('testing for colours', function() {
      
      it('should have a default colouring of no colour flag is given', function() {
        var nodes = [{_id: 1}, {_id: 2}],
        shaper = new NodeShaper(d3.select("svg"));
        shaper.drawNodes(nodes);
        
        expect($("#1").attr("stroke")).toEqual("#8AA051");
        expect($("#1").attr("fill")).toEqual("#FF8F35");
        expect($("#2").attr("stroke")).toEqual("#8AA051");
        expect($("#2").attr("fill")).toEqual("#FF8F35");
      });
      
      it('should be able to use the same colour for all nodes', function() {
        var nodes = [{_id: 1}, {_id: 2}],
        shaper = new NodeShaper(d3.select("svg"),
        {
          color: {
            type: "single",
            fill: "#123456",
            stroke: "#654321"
          }
        });
        shaper.drawNodes(nodes);
        
        expect($("#1").attr("fill")).toEqual("#123456");
        expect($("#1").attr("stroke")).toEqual("#654321");
        expect($("#2").attr("fill")).toEqual("#123456");
        expect($("#2").attr("stroke")).toEqual("#654321");
        
      });
      
      it('should be able to use a colour based on attribute value', function() {
        var nodes = [
          {
            _id: 1,
            _data: {
              label: "lbl1"
            }
          }, {
            _id: 2,
            _data: {
              label: "lbl2"
            }
          }, {
            _id: 3,
            _data: {
              label: "lbl3"
            }
          }, {
            _id: 4,
            _data: {
              label: "lbl1"
            }
          }],
        shaper = new NodeShaper(d3.select("svg"),
        {
          color: {
            type: "attribute",
            key: "label"
          }
        }),
        c1f, c2f, c3f, c4f,
        c1s, c2s, c3s, c4s;
        shaper.drawNodes(nodes);
        
        c1f = $("#1").attr("fill");
        c2f = $("#2").attr("fill");
        c3f = $("#3").attr("fill");
        c4f = $("#4").attr("fill");
        
        c1s = $("#1").attr("stroke");
        c2s = $("#2").attr("stroke");
        c3s = $("#3").attr("stroke");
        c4s = $("#4").attr("stroke");
        
        expect(c1f).toBeDefined();
        expect(c2f).toBeDefined();
        expect(c3f).toBeDefined();
        expect(c4f).toBeDefined();
        
        expect(c1f).toEqual(c4f);
        expect(c1f).not.toEqual(c2f);
        expect(c1f).not.toEqual(c3f);
        expect(c2f).not.toEqual(c3f);
        
        expect(c1s).toBeDefined();
        expect(c2s).toBeDefined();
        expect(c3s).toBeDefined();
        expect(c4s).toBeDefined();
        
        expect(c1s).toEqual(c4s);
        expect(c1s).not.toEqual(c2s);
        expect(c1s).not.toEqual(c3s);
        expect(c2s).not.toEqual(c3s);
      });
      
      it('should be able to use colours based on _expanded attribute', function() {
        var nodes = [
          {
            _id: 1,
            _expanded: true
          }, {
            _id: 2,
            _expanded: false
          }, {
            _id: 3
          }],
        shaper = new NodeShaper(d3.select("svg"),
        {
          color: {
            type: "expand",
            expanded: "#123456",
            collapsed: "#654321"
          }
        }),
        c1s, c2s, c3s,
        c1f, c2f, c3f;
        shaper.drawNodes(nodes);
        
        c1f = $("#1").attr("fill");
        c2f = $("#2").attr("fill");
        c3f = $("#3").attr("fill");
        
        c1s = $("#1").attr("stroke");
        c2s = $("#2").attr("stroke");
        c3s = $("#3").attr("stroke");
        
        expect(c1f).toBeDefined();
        expect(c2f).toBeDefined();
        expect(c3f).toBeDefined();
        
        expect(c1f).toEqual("#123456");
        expect(c2f).toEqual("#654321");
        expect(c3f).toEqual("#654321");
        
        expect(c1s).toBeDefined();
        expect(c2s).toBeDefined();
        expect(c3s).toBeDefined();
        
        expect(c1s).toEqual("#123456");
        expect(c2s).toEqual("#654321");
        expect(c3s).toEqual("#654321");
      });
      
    });

    describe('when nodes are already drawn', function() {
      var nodes,
      clicked,
      click = function (node) {
        clicked[node._id] = !clicked[node._id];
      },
      shaper;
      
      beforeEach(function() {
        nodes = [{
          _id: 1,
          _data: {
            
          }
        }, {
          _id: 2,
          _data: {
          
          }
        }, {
          _id: 3,
          _data: {
          
          }
        }];
        clicked = [];
        shaper = new NodeShaper(d3.select("svg"));
        shaper.drawNodes(nodes);
        expect($("svg .node").length).toEqual(3);
      });
      
      it('should be able to change display formats', function() {
        expect($("svg circle").length).toEqual(3);
        expect($("svg rect").length).toEqual(0);
        shaper.changeTo({shape: {type: NodeShaper.shapes.NONE}});
        expect($("svg circle").length).toEqual(0);
        expect($("svg rect").length).toEqual(0);
        shaper.changeTo({shape: {type: NodeShaper.shapes.RECT, width: 12, height: 5}});
        expect($("svg circle").length).toEqual(0);
        expect($("svg rect").length).toEqual(3);
        shaper.changeTo({shape: {type: NodeShaper.shapes.NONE}});
        expect($("svg circle").length).toEqual(0);
        expect($("svg rect").length).toEqual(0);
      });
      
      it('should be able to add a click event to existing nodes', function() {
        expect($("svg .node").length).toEqual(3);
        shaper.changeTo({actions: {
          click: click
        }});
        helper.simulateMouseEvent("click", "1");
        helper.simulateMouseEvent("click", "3");
        expect($("svg .node").length).toEqual(3);
        expect(clicked[1]).toBeTruthy();
        expect(clicked[3]).toBeTruthy();
        expect(clicked[2]).toBeUndefined();
      });
      
      it('should add a click event to newly arriving nodes', function() {
        
        shaper.changeTo({actions: {
          click: click
        }});
        nodes.push({_id: 4});
        nodes.push({_id: 5});
        shaper.drawNodes(nodes);
        
        helper.simulateMouseEvent("click", "4");
        helper.simulateMouseEvent("click", "5");
        expect($("svg .node").length).toEqual(5);
        expect(clicked[4]).toBeTruthy();
        expect(clicked[5]).toBeTruthy();
        expect(clicked[1]).toBeUndefined();
        expect(clicked[2]).toBeUndefined();
        expect(clicked[3]).toBeUndefined();
      });
      
      it('should be able to reset bound events', function() {
        shaper.changeTo({
          actions: {
            click: click
          }
        });
        shaper.drawNodes(nodes);
        
        helper.simulateMouseEvent("click", "1");
        
        shaper.changeTo({actions: {
          reset: true
        }});
        
        helper.simulateMouseEvent("click", "2");
        
        expect(clicked[1]).toBeTruthy();
        expect(clicked[2]).toBeUndefined();
        expect(clicked[3]).toBeUndefined();
      });
      
      it('should be able to reset the drag event', function() {
        var dragged = 0,
          dragTest = function() {
            this.on("mousedown.drag", function() {
              dragged++;
            }).on("touchstart.drag", function() {
              dragged++;
            });
          };
        
        shaper.changeTo({
          actions: {
            drag: dragTest
          }
        });
        shaper.drawNodes(nodes);

        helper.simulateDragEvent("1");
        
        expect(dragged).toEqual(1);
        
        shaper.changeTo({actions: {
          reset: true
        }});
        
        helper.simulateDragEvent("1");
        helper.simulateDragEvent("2");
        helper.simulateDragEvent("3");
        
        expect(dragged).toEqual(1);
      });
      
      
      it('should display each node exactly once if an event is added', function() {
        shaper.changeTo({actions: {
          click: function() {return 0;}
        }});
        expect($("svg .node").length).toEqual(3);
        shaper.changeTo({actions: {
          click: function() {return 1;}
        }});
        expect($("svg .node").length).toEqual(3);
        shaper.changeTo({actions: {
          click: function() {return 2;}
        }});
        expect($("svg .node").length).toEqual(3);
      });
      
      it('should be able to reshape a single node only', function() {
        shaper.changeTo({label: "label"});
        nodes[0]._data.label = "false";
        nodes[1]._data.label = "true";
        nodes[2]._data.label = "false_again";
        expect($("#1 text").length).toEqual(1);
        expect($("#2 text").length).toEqual(1);
        expect($("#3 text").length).toEqual(1);
        expect($("#1 text")[0].textContent).toEqual("");
        expect($("#2 text")[0].textContent).toEqual("");
        expect($("#3 text")[0].textContent).toEqual("");
        
        shaper.reshapeNode(nodes[1]);
        expect($("#1 text").length).toEqual(1);
        expect($("#3 text").length).toEqual(1);
        expect($("#1 text")[0].textContent).toEqual("");
        expect($("#3 text")[0].textContent).toEqual("");
        
        expect($("#2 text").length).toEqual(1);
        expect($("#2 text")[0].textContent).toEqual("true");        
      });
    });

    describe('configured for circle', function () {
      var shaper;

      beforeEach(function () {
        var config = {
          shape: {
            type: NodeShaper.shapes.CIRCLE
          }
        };
        shaper = new NodeShaper(d3.select("svg"), config);
      });

      it('should draw circle elements', function () {
        var node = [{_id: 1}];
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
          shape: {
            type: NodeShaper.shapes.CIRCLE,
            radius: 15
          }
        });
        var nodes = [
          {_id: 1},
          {_id: 2},
          {_id: 3},
          {_id: 4},
          {_id: 5}
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
          return 10 + node._id;
        },
        nodes = [
          {_id: 1},
          {_id: 2},
          {_id: 3},
          {_id: 4},
          {_id: 5}
        ];
        shaper = new NodeShaper(d3.select("svg"),
        {
          shape: {
            type: NodeShaper.shapes.CIRCLE,
            radius: radiusFunction
          } 
        });
        shaper.drawNodes(nodes);
        expect($("svg #1 circle")[0].attributes.r.value).toEqual("11");
        expect($("svg #2 circle")[0].attributes.r.value).toEqual("12");
        expect($("svg #3 circle")[0].attributes.r.value).toEqual("13");
        expect($("svg #4 circle")[0].attributes.r.value).toEqual("14");
        expect($("svg #5 circle")[0].attributes.r.value).toEqual("15");
        
      });
    
      it('should display each node exactly once if an event is added', function() {
        var nodes = [
          {_id: 1},
          {_id: 2},
          {_id: 3},
          {_id: 4},
          {_id: 5}
        ];
        shaper.drawNodes(nodes);
        
        expect($("svg circle").length).toEqual(5);
        shaper.changeTo({actions: {
          click: function() {return 0;}
        }});
        expect($("svg circle").length).toEqual(5);
        shaper.changeTo({actions: {
          click: function() {return 1;}
        }});
        expect($("svg circle").length).toEqual(5);
        shaper.changeTo({actions: {
          click: function() {return 2;}
        }});
        expect($("svg circle").length).toEqual(5);
      });
    
    });
    
    describe('configured for rectangle', function () {
      var shaper;

      beforeEach(function () {
        shaper = new NodeShaper(d3.select("svg"), {
          shape: {
            type: NodeShaper.shapes.RECT
          }
        });
      });

      it('should draw rect elements', function () {
        var node = [{_id: 1}];
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
          shape: {
            type: NodeShaper.shapes.RECT,
            width: 15,
            height: 10
          }
        });
        var nodes = [
          {_id: 1},
          {_id: 2},
          {_id: 3},
          {_id: 4},
          {_id: 5}
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
          return 10 + node._id;
        },
        heightFunction = function (node) {
          return 10 - node._id;
        },
        nodes = [
          {_id: 1},
          {_id: 2},
          {_id: 3},
          {_id: 4},
          {_id: 5}
        ];
        shaper = new NodeShaper(d3.select("svg"),
        {
          shape:  {
            type: NodeShaper.shapes.RECT,
            width: widthFunction,
            height: heightFunction
          }
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
          "label": "label"
        });
      });

      it('should add a text element', function () {
        var node = [{
          _id: 1,
          _data: {
            "label": "MyLabel"
          }
        }];
        shaper.drawNodes(node);
        expect($("svg .node")[0]).toBeDefined();
        expect($("svg .node").length).toEqual(1);
        expect($("svg #1")[0]).toBeDefined();
        expect($("svg text")[0]).toBeDefined();
        expect($("svg text").length).toEqual(1);
        expect($("svg .node text")[0]).toBeDefined();
        expect($("svg .node text").length).toEqual(1);
        expect($("svg .node text")[0].textContent).toEqual("MyLabel");
        expect($("svg .node text").attr("stroke")).toEqual("black");
        expect($("svg .node text").attr("fill")).toEqual("black");
      });

      it('should ignore other attributes', function () {
        var nodes = [
          {
            _id: 1, 
            _data: {
              "label": "correct"
            }
          },
          {
            _id: 2, 
            _data: {
              "alt": "incorrect"
            }
          },
          {
            _id: 3, 
            _data: {
              "alt": "incorrect"
            }
          },
          {
            _id: 4, 
            _data: {
              "label": "correct",
              "alt": "incorrect"
            }
          }];
        shaper.drawNodes(nodes);
        expect($("svg #1 text")[0].textContent).toEqual("correct");
        expect($("svg #2 text")[0].textContent).toEqual("");
        expect($("svg #3 text")[0].textContent).toEqual("");
        expect($("svg #4 text")[0].textContent).toEqual("correct");
      });

      it('should also print "0" as a label', function() {
        var node = [{
          _id: 1,
          _data: {
            "label": 0
          }
        }];
        shaper.drawNodes(node);
        expect($("svg .node text")[0]).toBeDefined();
        expect($("svg .node text").length).toEqual(1);
        expect($("svg .node text")[0].textContent).toEqual("0");
      });

      it('should be able to switch to another label during runtime', function() {
        var node = [{
          _id: 1,
          _data: {
            label: "before",
            switched: "after"
          }
        }];
        shaper.drawNodes(node);
        expect($("svg .node text")[0].textContent).toEqual("before");
        shaper.changeTo({label: "switched"});
        expect($("svg .node text")[0]).toBeDefined();
        expect($("svg .node text").length).toEqual(1);
        expect($("svg .node text")[0].textContent).toEqual("after");
        
      });

    });

    describe('using a function for labels', function () {
      var shaper,
      labelFunction = function (node) {
        if (node._id === 4) {
          return "correct";
        }
        if (node._data.label) {
          return node._data.label;
        }
        if (node._data.alt) {
          return node._data.alt;
        }
        return "default";
      };

      beforeEach(function () {
        shaper = new NodeShaper(d3.select("svg"), {
          "label": labelFunction
        });
      });

      it('should display the correct labels according to the function', function () {
        var nodes = [
          {
            _id: 1,
            _data: {
              "label": "correct"
            }
          },
          {
            _id: 2,
            _data: {
              "alt": "correct"
            }
          },
          {
            _id: 3,
            _data: {
              "label": "correct",
              "alt": "incorrect"
            }
          },
          {
            _id: 4,
            _data: {
              "label": "incorrect",
              "alt": "incorrect"
            }
          },
          {
            _id: 5,
            _data: {}
          }
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
        nodes = [{_id: 1}, {_id: 2}, {_id: 3}, {_id: 4}, {_id: 5}];
        shaper.drawNodes(nodes);
      });

      it('should be able to draw more nodes', function () {
        nodes.push({_id: 6});
        nodes.push({_id: 7});
        nodes.push({_id: 8});

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
        nodes.push({_id: 6});
        nodes.push({_id: 7});
        nodes.push({_id: 8});
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
          node = [{_id: 1}],
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
              shape: {
                type: NodeShaper.shapes.CIRCLE
              },
              label: "label"
            }
          );
      });

      it('should draw circle elements', function () {
        var node = [{
          _id: 1,
          _data: {
            "label": "correct"
          }
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
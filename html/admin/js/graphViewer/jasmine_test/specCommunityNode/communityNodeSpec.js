/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach, jasmine*/
/*global describe, it, expect, spyOn */
/*global helper*/
/*global document, window*/
/*global CommunityNode*/

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

  describe('Community Node', function() {
    
    var nodes;
    
    beforeEach(function () {
      nodes = [];
      helper.insertNSimpleNodes(nodes, 30);
    });
    
    describe('checking setup', function() {
      
      it('should throw an error if no parent is given', function() {
        expect(
          function() {
            var t = new CommunityNode();
          }
        ).toThrow("A parent element has to be given.");
      });
      
      it('should not throw an error if a parent is given', function() {
        expect(
          function() {
            var t = new CommunityNode({
              dissolveCommunity: function() {}
            });
          }
        ).not.toThrow();
      });
      
    });
    
    describe('checking the interface', function() {
      
      var testee;
      
      beforeEach(function() {
        var parent = {
          dissolveCommunity: function() {}
        };
        testee = new CommunityNode(parent, nodes.slice(3, 13));
        this.addMatchers({
          toHaveFunction: function(func, argCounter) {
            var obj = this.actual;
            this.message = function(){
              return testee.constructor.name
                + " should react to function "
                + func
                + " with at least "
                + argCounter
                + " arguments.";
            };
            if (typeof(obj[func]) !== "function") {
              return false;
            }
            if (obj[func].length < argCounter) {
              return false;
            }
            return true;
          }
        });
      });
      
      it('should offer a function to getAll included nodes', function() {
        expect(testee).toHaveFunction("getNodes", 0);
      });
      
      it('should offer a function to check if a node is included', function() {
        expect(testee).toHaveFunction("hasNode", 1);
      });
      
      it('should offer a function to get a node if it is included', function() {
        expect(testee).toHaveFunction("getNode", 1);
      });
      
      it('should offer a function to insert an additional node', function() {
        expect(testee).toHaveFunction("insertNode", 1);
      });
      
      it('should offer a function to insert an incomming edge', function() {
        expect(testee).toHaveFunction("insertInboundEdge", 1);
      });
      
      it('should offer a function to insert an outgoing edge', function() {
        expect(testee).toHaveFunction("insertOutboundEdge", 1);
      });
      
      it('should offer a function to remove in incomming edge', function() {
        expect(testee).toHaveFunction("removeInboundEdge", 1);
      });
      
      it('should offer a function to remove in outgoing edge', function() {
        expect(testee).toHaveFunction("removeOutboundEdge", 1);
      });
      
      it('should offer a function to remove a node', function() {
        expect(testee).toHaveFunction("removeNode", 1);
      });
      
      it('should offer a function to remove and return all outgoing edges of a node', function() {
        expect(testee).toHaveFunction("removeOutboundEdgesFromNode", 1);
      });
            
      it('should offer a function to get the dissolve info', function() {
        expect(testee).toHaveFunction("getDissolveInfo", 0);
      });
      
      it('should offer a function to shape the community', function() {
        expect(testee).toHaveFunction("shape", 3);
      });
      
      it('should offer a function to expand the community', function() {
        expect(testee).toHaveFunction("expand", 0);
      });
      
      it('should offer a function to collapse the community', function() {
        expect(testee).toHaveFunction("collapse", 0);
      });
      
      it('should offer a function to dissolve the community', function() {
        expect(testee).toHaveFunction("dissolve", 0);
      });
    });
    
    describe('node functionality', function() {
      
      var parent;
      
      beforeEach(function() {
        parent = {
          dissolveCommunity: function() {}
        };
      });
      
      it('should create a communityNode containing the given nodes', function() {
        var c = new CommunityNode(parent, nodes.slice(3, 13));
        expect(c.getNodes()).toEqual(nodes.slice(3, 13));
      });
      
      it('should be able to insert a new node', function() {
        var c = new CommunityNode(parent, nodes.slice(3, 13)),
          n = {
          _id: "foxx",
          _inboundCounter: 0,
          _outboundCounter: 0,
          position: {
            x: 1,
            y: 1,
            z: 1
          }
        };
        c.insertNode(n);
        expect(c.getNodes()).toEqual(nodes.slice(3, 13).concat([n]));
      });
    
      it('should be able to check if a node is included', function() {
        var n = {
          _id: "foxx",
          _inboundCounter: 0,
          _outboundCounter: 0,
          position: {
            x: 1,
            y: 1,
            z: 1
          }
        },
        c = new CommunityNode(parent, [n]);
      
        expect(c.hasNode("foxx")).toBeTruthy();
        expect(c.hasNode("1")).toBeFalsy();
      });
    
      it('should only acknowledge included nodes', function() {
        var c = new CommunityNode(parent, nodes.slice(3, 13)),
          i;
        for (i = 0; i < nodes.length; i++) {
          if (2 < i && i < 13) {
            expect(c.hasNode(nodes[i]._id)).toBeTruthy();
          } else {
            expect(c.hasNode(nodes[i]._id)).toBeFalsy();
          }
        }
      });
    
      it('should return nodes by id', function() {
        var n = {
          _id: "foxx",
          _inboundCounter: 0,
          _outboundCounter: 0,
          position: {
            x: 1,
            y: 1,
            z: 1
          }
        },
        c = new CommunityNode(parent, [n]);
        expect(c.getNode("foxx")).toEqual(n);
      });
    
      it('should return undefined if node is not contained', function() {
        var n = {
          _id: "foxx",
          _inboundCounter: 0,
          _outboundCounter: 0,
          position: {
            x: 1,
            y: 1,
            z: 1
          }
        },
        c = new CommunityNode(parent, [n]);
        expect(c.getNode("nofoxx")).toBeUndefined();
      });
    
      it('should be able to remove a node', function() {
        var c = new CommunityNode(parent, nodes.slice(3, 5)),
          n = {
          _id: "foxx",
          _inboundCounter: 0,
          _outboundCounter: 0,
          position: {
            x: 1,
            y: 1,
            z: 1
          }
        };
        expect(c._size).toEqual(2);
        c.insertNode(n);
        expect(c._size).toEqual(3);
        c.removeNode(n);
        expect(c._size).toEqual(2);
        expect(c.getNodes()).toEqual(nodes.slice(3, 5));
      });
    
      it('should be able to remove a node by id', function() {
        var c = new CommunityNode(parent, nodes.slice(3, 5)),
          n = {
          _id: "foxx",
          _inboundCounter: 0,
          _outboundCounter: 0,
          position: {
            x: 1,
            y: 1,
            z: 1
          }
        };
        expect(c._size).toEqual(2);
        c.insertNode(n);
        expect(c._size).toEqual(3);
        c.removeNode("foxx");
        expect(c._size).toEqual(2);
        expect(c.getNodes()).toEqual(nodes.slice(3, 5));
      });
      
    });
    
    describe('shaping functionality', function() {
      
      var tSpan1, tSpan2, text, g, shaper, colourMapper, box, boxRect,
        parent, c;
        
      beforeEach(function() {
        parent = {
          dissolveCommunity: function() {}
        };
        var first = true;
        box = {
          x: -10,
          y: -10,
          width: 20,
          height: 20
        };
        tSpan1 = {
          attr: function() {
            return this;
          },
          text: function() {
            return this;
          }
        };
        tSpan2 = {
          attr: function() {
            return this;
          },
          text: function() {
            return this;
          }
        };
        boxRect = {
          attr: function() {
            return this;
          }
        };
        text = {
          attr: function() {
            return this;
          },
          text: function() {
            return this;
          },
          append: function() {
            if (first) {
              first = false;
              return tSpan1;
            }
            return tSpan2;
            
          }
        };
        g = {
          attr: function() {
            return this;
          },
          append: function(type) {
            if (type === "text") {
              return text;
            }
            if (type === "rect") {
              return boxRect;
            }
          }
        };
        shaper = {
          shapeFunc: function() {}
        };
        colourMapper = {
          getForegroundCommunityColour: function() {
            return "black";
          }
        };
        
        spyOn(document, "getElementById").andCallFake(function() {
          return {
            getBBox: function() {
              return box;
            }
          };
        });
        
        c = new CommunityNode(parent, nodes.slice(0, 5));
      });
      
      it('should initially contain the required attributes for shaping', function() {
        var x = 42,
          y = 23,
          n = {
            _id: "foxx",
            _inboundCounter: 0,
            _outboundCounter: 0,
            x: x,
            y: y,
            z: 1,
            position: {
              x: x,
              y: y,
              z: 1
            }
          },
          otherC = new CommunityNode(parent, [n].concat(nodes.slice(5,21)));
        expect(otherC.x).toBeDefined();
        expect(otherC.x).toEqual(x);
        expect(otherC.y).toBeDefined();
        expect(otherC.y).toEqual(y);
        expect(otherC._size).toEqual(17);
        expect(otherC._isCommunity).toBeTruthy();
      });
       
      it('should shape the collapsed community with given functions', function() {
        spyOn(g, "attr").andCallThrough();
        spyOn(g, "append").andCallThrough();
        spyOn(shaper, "shapeFunc").andCallThrough();
        spyOn(colourMapper, "getForegroundCommunityColour").andCallThrough();
        
        c.shape(g, shaper.shapeFunc, colourMapper);
        
        expect(colourMapper.getForegroundCommunityColour).wasCalled();
        expect(g.attr).wasCalledWith("stroke", "black");
        expect(shaper.shapeFunc).wasCalledWith(g, 9);
        expect(shaper.shapeFunc).wasCalledWith(g, 6);
        expect(shaper.shapeFunc).wasCalledWith(g, 3);
        expect(shaper.shapeFunc).wasCalledWith(g);
      });
      
      it('should add a label containing the size of a community', function() {
        spyOn(g, "append").andCallThrough();
        spyOn(text, "attr").andCallThrough();
        spyOn(text, "text").andCallThrough();
        spyOn(text, "append");
        spyOn(colourMapper, "getForegroundCommunityColour").andCallThrough();
        c.shape(g, shaper.shapeFunc, colourMapper);
        
        expect(g.append).wasCalledWith("text");
        expect(text.attr).wasCalledWith("text-anchor", "middle");
        expect(text.attr).wasCalledWith("fill", "black");
        expect(text.attr).wasCalledWith("stroke", "none");
        expect(text.text).wasCalledWith(5);
        expect(text.append).wasNotCalled();        
      });
      
      it('should add a label if a reason is given', function() {
        c._reason = {
          key: "key",
          value: "label"
        };
        
        spyOn(g, "append").andCallThrough();
        spyOn(text, "attr").andCallThrough();
        spyOn(text, "append").andCallThrough();
        spyOn(text, "text");
        spyOn(tSpan1, "attr").andCallThrough();
        spyOn(tSpan1, "text").andCallThrough();
        spyOn(tSpan2, "attr").andCallThrough();
        spyOn(tSpan2, "text").andCallThrough();
        spyOn(colourMapper, "getForegroundCommunityColour").andCallThrough();
        c.shape(g, shaper.shapeFunc, colourMapper);
        
        expect(g.append).wasCalledWith("text");
        expect(text.attr).wasCalledWith("text-anchor", "middle");
        expect(text.attr).wasCalledWith("fill", "black");
        expect(text.attr).wasCalledWith("stroke", "none");
        expect(text.text).wasNotCalled();
        expect(text.append).wasCalledWith("tspan");
        expect(text.append.calls.length).toEqual(2);
        expect(tSpan1.attr).wasCalledWith("x", "0");
        expect(tSpan1.attr).wasCalledWith("dy", "-4");
        expect(tSpan1.text).wasCalledWith("key:");
        
        expect(tSpan2.attr).wasCalledWith("x", "0");
        expect(tSpan2.attr).wasCalledWith("dy", "16");
        expect(tSpan2.text).wasCalledWith("label");        
      });
      
      describe('if the community is expanded', function() {
        
        var nodeSelector, interior, iEnter, iExit, iAll, iG,
        observer, observerCB;
        
        beforeEach(function() {
          g.selectAll = function(selector) {
            return nodeSelector;
          };
          nodeSelector = {
            data: function() {
              return interior;
            }
          };
          interior = {
            enter: function() {
              return iEnter;
            },
            exit: function() {
              return iExit;
            },
            selectAll: function() {
              return iAll;
            },
            attr: function() {
              return this;
            }
          };
          iEnter = {
            append: function() {
              return iG;
            }
          };
          iG = {
            attr: function() {
              return this;
            }
          };
          iExit = {
            remove: function() {}
          };
          iAll = {
            remove: function() {}
          };
          observer = {
            observe: function() {},
            disconnect: function() {}
          };
          spyOn(window, "WebKitMutationObserver").andCallFake(function(cb) {
            observerCB = cb;
            return observer;
          });
          
          c.expand();
        });
        
        
        it('should print the bounding box correctly', function() {
          spyOn(g, "append").andCallThrough();
          spyOn(boxRect, "attr").andCallThrough();
        
          c.shape(g, shaper.shapeFunc, colourMapper);
        
          expect(g.append).wasCalledWith("rect");
          expect(boxRect.attr).wasCalledWith("rx", "8");
          expect(boxRect.attr).wasCalledWith("ry", "8");
          expect(boxRect.attr).wasCalledWith("fill", "none");
          expect(boxRect.attr).wasCalledWith("stroke", "black");
          
        });
        
        it('should update the box as soon as the dom is ready', function() {
          spyOn(boxRect, "attr").andCallThrough();
          spyOn(observer, "observe").andCallThrough();
          spyOn(observer, "disconnect").andCallThrough();
          
          c.shape(g, shaper.shapeFunc, colourMapper);
          
          expect(document.getElementById).wasCalledWith(c._id);
          expect(observer.observe).wasCalledWith(
            jasmine.any(Object),
            {
              subtree:true,
              attributes:true
            }
          );
          
          
          observerCB([{attributeName: "transform"}]);
          
          expect(boxRect.attr).wasCalledWith("width", box.width + 10);
          expect(boxRect.attr).wasCalledWith("height", box.height + 10);
          expect(boxRect.attr).wasCalledWith("x", box.x - 5);
          expect(boxRect.attr).wasCalledWith("y", box.y - 5);
          expect(observer.disconnect).wasCalled();
        });
        
        it('should shape the expanded community with given functions', function() {

          spyOn(g, "selectAll").andCallThrough();
          spyOn(nodeSelector, "data").andCallFake(function(a, func) {
            expect(func(nodes[0])).toEqual(nodes[0]._id);
            expect(func(nodes[6])).toEqual(nodes[6]._id);
            return interior;
          });
          spyOn(interior, "enter").andCallThrough();
          spyOn(interior, "exit").andCallThrough();
          spyOn(interior, "selectAll").andCallThrough();
          spyOn(iEnter, "append").andCallThrough();
          spyOn(iG, "attr").andCallThrough();
          spyOn(iExit, "remove").andCallThrough();
          spyOn(iAll, "remove").andCallThrough();
          
          
          c.shape(g, shaper.shapeFunc, colourMapper);
          
          expect(g.selectAll).wasCalledWith(".node");
          expect(nodeSelector.data).wasCalledWith(c.getNodes(), jasmine.any(Function));
          expect(interior.enter).wasCalled();
          expect(iEnter.append).wasCalledWith("g");
          expect(iG.attr).wasCalledWith("class", "node");
          expect(iG.attr).wasCalledWith("id", jasmine.any(Function));
          expect(interior.exit).wasCalled();
          expect(iExit.remove).wasCalled();
          expect(interior.selectAll).wasCalledWith("* > *");
          expect(iAll.remove).wasCalled();

          /*
          var observer = new WebKitMutationObserver(function(e){
            if (_.any(e, function(obj) {
              return obj.attributeName === "transform";
            })) {
              updateBoundingBox();
              observer.disconnect();
            }
          });
          observer.observe(document.getElementById(self._id), {
            subtree:true,
            attributes:true
          });
          addShape(interior, shapeFunc, colourMapper);
          */
        });
        
        it('should apply distortion on the interior nodes', function() {
          // Fake Layouting to test correctness
          /*
          nodes[0].x = -20;
          nodes[0].y = 20;
          nodes[1].x = -10;
          nodes[1].y = 10;
          nodes[2].x = 0;
          nodes[2].y = 0;
          nodes[3].x = 10;
          nodes[3].y = -10;
          nodes[4].x = 20;
          nodes[4].y = -20;
          */
        });
        
      });
      
      
    });
    
    describe('edge functionality', function() {
      
      var parent;
      
      beforeEach(function() {
        parent = {
          dissolveCommunity: function() {}
        };
      });
      
      it('should return true if an inserted edge is internal', function() {
        var c = new CommunityNode(parent), 
          e1 = {
            _id: "1-2",
            _data: {
              _from: "1",
              _to: "2"
            },
            source: nodes[1],
            target: nodes[2]
          },
          e2 = {
            _id: "2-1",
            _data: {
              _from: "2",
              _to: "1"
            },
            source: nodes[2],
            target: nodes[1]
          };
        c.insertInboundEdge(e1);
        expect(c.insertOutboundEdge(e1)).toBeTruthy();
        c.insertOutboundEdge(e2);
        expect(c.insertInboundEdge(e2)).toBeTruthy();
      });
    
      it('should return false if an inserted edge is not internal', function() {
        var c = new CommunityNode(parent), 
          e1 = {
            _id: "1-2",
            _data: {
              _from: "1",
              _to: "2"
            },
            source: nodes[1],
            target: nodes[2]
          },
          e2 = {
            _id: "2-1",
            _data: {
              _from: "2",
              _to: "1"
            },
            source: nodes[2],
            target: nodes[1]
          };
        expect(c.insertInboundEdge(e1)).toBeFalsy();
        expect(c.insertOutboundEdge(e2)).toBeFalsy();
      });
    
      it('should be possible to remove an inbound edge', function() {
        var c = new CommunityNode(parent), 
          e = {
            _id: "1-2",
            _data: {
              _from: "1",
              _to: "2"
            },
            source: nodes[1],
            target: nodes[2]
          };
        c.insertInboundEdge(e);
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [e],
          outbound: [],
          both: []
        });
      
        expect(e.target).toEqual(c);
        expect(e._target).toEqual(nodes[2]);
      
        expect(c._inboundCounter).toEqual(1);
      
        c.removeInboundEdge(e);
      
        expect(c._inboundCounter).toEqual(0);
      
        expect(e.target).toEqual(nodes[2]);
      
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [],
          outbound: [],
          both: []
        });
      });
    
      it('should be possible to remove an inbound edge by its id', function() {
        var c = new CommunityNode(parent), 
          e = {
            _id: "1-2",
            _data: {
              _from: "1",
              _to: "2"
            },
            source: nodes[1],
            target: nodes[2]
          };
        c.insertInboundEdge(e);
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [e],
          outbound: [],
          both: []
        });
      
        expect(e.target).toEqual(c);
      
        c.removeInboundEdge("1-2");
      
        expect(e.target).toEqual(nodes[2]);
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [],
          outbound: [],
          both: []
        });
      });
    
      it('should be possible to remove the inbound value of an internal edge', function() {
        var c = new CommunityNode(parent), 
          e = {
            _id: "1-2",
            _data: {
              _from: "1",
              _to: "2"
            },
            source: nodes[1],
            target: nodes[2]
          };
        c.insertInboundEdge(e);
        c.insertOutboundEdge(e);
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [],
          outbound: [],
          both: [e]
        });
      
        expect(c._outboundCounter).toEqual(0);
        expect(c._inboundCounter).toEqual(0);
        c.removeInboundEdge(e);
        expect(c._outboundCounter).toEqual(1);
        expect(c._inboundCounter).toEqual(0);
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [],
          outbound: [e],
          both: []
        });
      });
    
      it('should be possible to remove an outbound edge', function() {
        var c = new CommunityNode(parent), 
          e = {
            _id: "1-2",
            _data: {
              _from: "1",
              _to: "2"
            },
            source: nodes[1],
            target: nodes[2]
          };
        c.insertOutboundEdge(e);
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [],
          outbound: [e],
          both: []
        });
      
        expect(e.source).toEqual(c);
      
        expect(c._outboundCounter).toEqual(1);
        c.removeOutboundEdge(e);
        expect(c._outboundCounter).toEqual(0);
      
        expect(e.source).toEqual(nodes[1]);
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [],
          outbound: [],
          both: []
        });
      });
    
      it('should be possible to remove an outbound edge by its id', function() {
        var c = new CommunityNode(parent), 
          e = {
            _id: "1-2",
            _data: {
              _from: "1",
              _to: "2"
            },
            source: nodes[1],
            target: nodes[2]
          };
        c.insertOutboundEdge(e);
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [],
          outbound: [e],
          both: []
        });
      
        expect(e.source).toEqual(c);
      
        c.removeOutboundEdge("1-2");
      
        expect(e.source).toEqual(nodes[1]);
      
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [],
          outbound: [],
          both: []
        });
      });
    
      it('should be possible to remove the outbound value of an internal edge', function() {
        var c = new CommunityNode(parent), 
          e = {
            _id: "1-2",
            _data: {
              _from: "1",
              _to: "2"
            },
            source: nodes[1],
            target: nodes[2]
          };
        c.insertInboundEdge(e);
        c.insertOutboundEdge(e);
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [],
          outbound: [],
          both: [e]
        });
      
        expect(c._outboundCounter).toEqual(0);
        expect(c._inboundCounter).toEqual(0);
        c.removeOutboundEdge(e);
        expect(c._outboundCounter).toEqual(0);
        expect(c._inboundCounter).toEqual(1);
      
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [e],
          outbound: [],
          both: []
        });
      });
    
      it('should be possible to return and remove all outbound edges for a node', function() {
        var c = new CommunityNode(parent, [nodes[0], nodes[1]]),
          e01 = {
            _id: "0-1",
            _data: {
              _from: "0",
              _to: "1"
            },
            source: nodes[0],
            target: nodes[1]
          },
          e02 = {
            _id: "0-2",
            _data: {
              _from: "0",
              _to: "2"
            },
            source: nodes[0],
            target: nodes[2]
          },
          e12 = {
            _id: "1-2",
            _data: {
              _from: "1",
              _to: "2"
            },
            source: nodes[1],
            target: nodes[2]
          };
        
        c.insertOutboundEdge(e01);
        c.insertOutboundEdge(e02);
        c.insertOutboundEdge(e12);
        c.insertInboundEdge(e01);
      
        expect(c.removeOutboundEdgesFromNode(nodes[0])).toEqual(
          [e01, e02]
        );
        expect(c.getDissolveInfo().edges).toEqual({
          inbound: [e01],
          outbound: [e12],
          both: []
        });
      
      });
      
    });
    
    describe('expansion functionality', function() {
      
      var parent;
      
      beforeEach(function() {
        parent = {
          dissolveCommunity: function() {}
        };
      });
      
      it('should be possible to dissolve the community', function() {
        var c = new CommunityNode(parent);
        spyOn(parent, "dissolveCommunity");
        c.dissolve();
        expect(parent.dissolveCommunity).wasCalledWith(c);
      });
      
      it('should be possible to expand the community', function() {
        var c = new CommunityNode(parent);
        expect(c._expanded).toBeFalsy();
        c.expand();
        expect(c._expanded).toBeTruthy();
      });
      
      it('should be possible to collapse the community', function() {
        var c = new CommunityNode(parent);
        expect(c._expanded).toBeFalsy();
        c.expand();
        expect(c._expanded).toBeTruthy();
        c.collapse();
        expect(c._expanded).toBeFalsy();
      });
      
    });
    
    describe('convenience methods', function() {
      
      var parent;
      
      beforeEach(function() {
        parent = {
          dissolveCommunity: function() {}
        };
      });
      
      it('should offer an attribute indicating that it is a community', function() {
        var c = new CommunityNode(parent, nodes.slice(0, 1));
        expect(c._isCommunity).toBeDefined();
        expect(c._isCommunity).toBeTruthy();
      });
      
      it('should be able to dissolve the community', function() {
        var c = new CommunityNode(parent, nodes.slice(3, 13)),
          e1 = {
            _id: "3-4",
            _data: {
              _from: "3",
              _to: "4"
            },
            source: nodes[3],
            target: nodes[4]
          },
          e2 = {
            _id: "1-7",
            _data: {
              _from: "1",
              _to: "7"
            },
            source: nodes[1],
            target: nodes[7]
          },
          e3 = {
            _id: "8-25",
            _data: {
              _from: "8",
              _to: "25"
            },
            source: nodes[8],
            target: nodes[25]
          };
        c.insertInboundEdge(e1);
        c.insertOutboundEdge(e1);
        c.insertInboundEdge(e2);
        c.insertOutboundEdge(e3);
        expect(c.getDissolveInfo()).toEqual({
          nodes: nodes.slice(3, 13),
          edges: {
            both: [e1],
            inbound: [e2],
            outbound: [e3]
          }
        });
        expect(e1.source).toEqual(c);
        expect(e1._source).toEqual(nodes[3]);
        expect(e1.target).toEqual(c);
        expect(e1._target).toEqual(nodes[4]);
        expect(e2.target).toEqual(c);
        expect(e2._target).toEqual(nodes[7]);
        expect(e3.source).toEqual(c);
        expect(e3._source).toEqual(nodes[8]);
      });
      
    });
    
  });

}());

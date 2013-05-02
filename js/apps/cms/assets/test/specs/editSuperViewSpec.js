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

var app = app || {};

(function () {
  "use strict";


  describe('Abstract Edit Dialog', function() {
    
    var waitTime = 1000;
    
    it('should bind itself to the app scope', function() {
      expect(app.EditSuperView).toBeDefined();
    });
    
    describe('checking extension', function() {
      
      var viewClass = app.EditSuperView.extend({
        save: function() {
          $('#document_modal').modal('hide');
        },
    
      	render: function () {
          this.superRender({columns: this.columns});
          return this;
      	}
      }),
      view;
      
      it('should open and close the dialog', function() {
        
        runs(function() {
          
          var columns = [];
          view = new viewClass({columns: columns});
          spyOn(view, "save");
          view.render();
        
          expect($("#document_modal").length).toEqual(1);
          expect($(".modal-backdrop").length).toEqual(1);
          expect($(".modal-backdrop").hasClass("in")).toBeTruthy();
          
          $(".modal-backdrop").click();
        });
        
        waitsFor(function() {
          return $(".modal-backdrop").length === 0;
        }, 1000, "The modal view should have been disappered.");
        
        runs(function() {
          expect($("#document_modal").length).toEqual(0);
          expect($(".modal-backdrop").length).toEqual(0);
          expect(view.save).wasNotCalled();
        });
        
      });
      
      
      it('should save on button click', function() {
        
        runs(function() {
          
          var columns = [];
          view = new viewClass({columns: columns});
          spyOn(view, "save").andCallThrough();
          view.render();
        
          $("#store").click();
          expect(view.save).toHaveBeenCalled();
        });
        
        waitsFor(function() {
          return $(".modal-backdrop").length === 0;
        }, 1000, "The modal view should have been disappered.");
        
        
        runs(function() {
          expect($("#document_modal").length).toEqual(0);
          expect($(".modal-backdrop").length).toEqual(0);
        });
        
      });
      
      it('should save on enter press', function() {
        
        runs(function() {
          
          var columns = [];
          view = new viewClass({columns: columns});
          spyOn(view, "save").andCallThrough();
          view.render();
        
          var e = $.Event("keypress");
          e.which = 13; //choose the one you want
          e.keyCode = 13;
          $("body").trigger(e);
          expect(view.save).toHaveBeenCalled();
        });
        
        waitsFor(function() {
          return $(".modal-backdrop").length === 0;
        }, 1000, "The modal view should have been disappered.");
        
        
        runs(function() {
          expect($("#document_modal").length).toEqual(0);
          expect($(".modal-backdrop").length).toEqual(0);
        });
        
      });
      
      it('should only save once on enter press', function() {
        
        runs(function() {
          var columns = [{
            name: "test",
            type: "string"
          }];
          view = new viewClass({columns: columns});
          spyOn(view, "save").andCallThrough();
          view.render();
        });
        
        waits(1000);
        
        runs(function() {
          $("#test").attr("value", "Meier");
          
        
          var e = $.Event("keypress");
          e.which = 13; //choose the one you want
          e.keyCode = 13;
          $("#document_form").trigger(e);
          
          
        });
        
        waitsFor(function() {
          return $(".modal-backdrop").length === 0;
        }, 1000, "The modal view should have been disappered.");
        
        
        runs(function() {
          
          expect($("#document_modal").length).toEqual(0);
          expect($(".modal-backdrop").length).toEqual(0);
          expect(view.save.calls.length).toEqual(1);
        });
        
      });
      
      describe('checking validation', function() {
        
        describe('checking numbers', function() {
          
          var view;
          
          beforeEach(function() {
            var columns = [{
              name: "val",
              type: "number"
            }];
            view = new viewClass({columns: columns});
            spyOn(view, "save").andCallThrough();
            view.render();
            
            // Sorry have to wait until modal and validation is active.
            waits(waitTime);
            
          });
          
          afterEach(function() {
            runs(function() {
              $(".modal-backdrop").click();;
            });
            
            waitsFor(function() {
              return $(".modal-backdrop").length === 0;
            }, 1000, "The modal view should have been disappered.");
            
          });
          
          it('should store integer numbers', function() {
            $("#val").attr("value", "1");
            $("#store").click();
            expect(view.save).toHaveBeenCalled();
          });
          
          it('should store decimal numbers', function() {
            $("#val").attr("value", "1.42");
            $("#store").click();
            expect(view.save).toHaveBeenCalled();
          });
          
          it('should store negative numbers', function() {
            $("#val").attr("value", "-1");
            $("#store").click();
            expect(view.save).toHaveBeenCalled();
          });
          
          it('should not store characters', function() {
            $("#val").attr("value", "1.a");
            $("#store").click();            
            expect(view.save).not.toHaveBeenCalled();
          });
          
          it('should not store NaN', function() {
            $("#val").attr("value", "NaN");
            $("#store").click();
            expect(view.save).not.toHaveBeenCalled();
          });
          
          it('should not store the empty string', function() {
            $("#val").attr("value", "");
            $("#store").click();
            expect(view.save).not.toHaveBeenCalled();
          });
          
        });
        
        describe('checking strings', function() {
          
          var view;
          
          beforeEach(function() {
            var columns = [{
              name: "val",
              type: "string"
            }];
            view = new viewClass({columns: columns});
            spyOn(view, "save").andCallThrough();
            view.render();
            
            // Sorry have to wait until modal and validation is active.
            waits(waitTime);
          });
          
          afterEach(function() {
            runs(function() {
              $(".modal-backdrop").click();;
            });
            
            waitsFor(function() {
              return $(".modal-backdrop").length === 0;
            }, 1000, "The modal view should have been disappered.");
            
          });
          
          it('should store any string', function() {
            $("#val").attr("value", "abc123");
            $("#store").click();
            expect(view.save).toHaveBeenCalled();
          });
          
          it('should not store the empty string', function() {
            $("#val").attr("value", "");
            $("#store").click();
            expect(view.save).not.toHaveBeenCalled();
          });
          
        });
        
        describe('checking email', function() {
          
          var view;
          
          beforeEach(function() {
            var columns = [{
              name: "val",
              type: "email"
            }];
            view = new viewClass({columns: columns});
            spyOn(view, "save").andCallThrough();
            view.render();
            
            // Sorry have to wait until modal and validation is active.
            waits(waitTime);
          });
          
          afterEach(function() {
            runs(function() {
              $(".modal-backdrop").click();;
            });
            
            waitsFor(function() {
              return $(".modal-backdrop").length === 0;
            }, 1000, "The modal view should have been disappered.");
            
          });
          
          it('should store a valid email', function() {
            $("#val").attr("value", "a@b.cd");
            $("#store").click();
            expect(view.save).toHaveBeenCalled();
          });
          
          it('should not store the empty string', function() {
            $("#val").attr("value", "");
            $("#store").click();
            expect(view.save).not.toHaveBeenCalled();
          });
          
          it('should not store a string without @', function() {
            $("#val").attr("value", "ab.cd");
            $("#store").click();
            expect(view.save).not.toHaveBeenCalled();
          });
          
          it('should not store a string without .', function() {
            $("#val").attr("value", "a@b");
            $("#store").click();
            expect(view.save).not.toHaveBeenCalled();
          });
          
          it('should not store a string with @ and . in reverse order', function() {
            $("#val").attr("value", "a.b@cd");
            $("#store").click();
            expect(view.save).not.toHaveBeenCalled();
          });
          
        });
        
      });
      
      describe('checking display', function() {
        
        afterEach(function() {
          runs(function() {
            $(".modal-backdrop").click();;
          });
          
          waitsFor(function() {
            return $(".modal-backdrop").length === 0;
          }, 1000, "The modal view should have been disappered.");
          
        });
        
        it('should display a text-field for numbers', function() {
          var columns = [{
            name: "val",
            type: "number"
          }],
            view = new viewClass({columns: columns});
          view.render();
          expect($("#val").is("input")).toBeTruthy();
          expect($("#val").attr("type")).toEqual("text");
        });
        
        it('should display a text-field for strings', function() {
          var columns = [{
            name: "val",
            type: "string"
          }],
            view = new viewClass({columns: columns});
          view.render();
          expect($("#val").is("input")).toBeTruthy();
          expect($("#val").attr("type")).toEqual("text");
        });
        
        it('should display a text-field for emails', function() {
          var columns = [{
            name: "val",
            type: "email"
          }],
            view = new viewClass({columns: columns});
          view.render();
          expect($("#val").is("input")).toBeTruthy();
          expect($("#val").attr("type")).toEqual("text");
        });
        
        it('should display a checkbox for booleans', function() {
          var columns = [{
            name: "val",
            type: "boolean"
          }],
            view = new viewClass({columns: columns});
          view.render();
          expect($("#val").is("input")).toBeTruthy();
          expect($("#val").attr("type")).toEqual("checkbox");
        });
        
      });
      
      describe('checking parser', function() {
        
        afterEach(function() {
          runs(function() {
            $(".modal-backdrop").click();;
          });
          
          waitsFor(function() {
            return $(".modal-backdrop").length === 0;
          }, 1000, "The modal view should have been disappered.");
          
        });
        
        it('should parse numbers', function() {
          var columns = [{
            name: "val",
            type: "number"
          }],
            view = new viewClass({columns: columns});
          view.render();
          $("#val").attr("value", "3.14");
          expect(view.parse()).toEqual({
            val: 3.14
          });
        });
        
        it('should parse strings', function() {
          var columns = [{
            name: "val",
            type: "string"
          }],
            view = new viewClass({columns: columns});
          view.render();
          $("#val").attr("value", "3.14");
          expect(view.parse()).toEqual({
            val: "3.14"
          });
        });
        
        it('should parse emails', function() {
          var columns = [{
            name: "val",
            type: "email"
          }],
            view = new viewClass({columns: columns});
          view.render();
          $("#val").attr("value", "a@b.cd");
          expect(view.parse()).toEqual({
            val: "a@b.cd"
          });
        });
        
        it('should parse booleans', function() {
          var columns = [{
            name: "val1",
            type: "boolean"
          },{
            name: "val2",
            type: "boolean"
          }],
            view = new viewClass({columns: columns});
          view.render();
          $("#val1").prop("checked", true);
          $("#val2").prop("checked", false);
          expect(view.parse()).toEqual({
            val1: true,
            val2: false
          });
        });
        
      });
    });
    
  });
}());

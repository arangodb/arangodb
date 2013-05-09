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

  describe('Content Collection Factory', function() {
    
    beforeEach(function() {
      app = app || {};
    });
    
    it('should bind itself to the app scope', function() {
      expect(app.ContentCollectionFactory).toBeDefined();
    });
    
    describe('given a struct URL', function() {
      
      var factory;
      
      beforeEach(function() {
        factory = new app.ContentCollectionFactory();
      });
      
      it('should be able to create a content collection for Strings', function() {
        
        var called;
        
        runs(function() {
          called = false;
          factory.create("String", function(collection) {
            expect(collection).toBeDefined();
            var firstModel = collection.create();
            expect(firstModel.get("name")).toBeDefined();
            expect(firstModel.get("name")).toEqual("");
            called = true;
          });
        });
         
        waitsFor(function() {
          return called;
        }, 2000, "Creation should have been called."); 
      
        runs(function() {
          expect(true).toBeTruthy();
        });    
      });
      
      it('should be able to create a content collection for Numbers', function() {
        
        var called;
        
        runs(function() {
          called = false;
          factory.create("Number", function(collection) {
            expect(collection).toBeDefined();
            var firstModel = collection.create();
            expect(firstModel.get("int")).toBeDefined();
            expect(firstModel.get("int")).toEqual(0);
            expect(firstModel.get("double")).toBeDefined();
            expect(firstModel.get("double")).toEqual(0);
            called = true;
          });
        });
        
        waitsFor(function() {
          return called;
        }, 2000, "Creation should have been called."); 
      
        runs(function() {
          expect(true).toBeTruthy();
        });
        
      });
      
      it('should be able to create a content collection for different attributes', function() {
        
        var called;
        
        runs(function() {
          called = false;
          factory.create("All", function(collection) {
            expect(collection).toBeDefined();
            var firstModel = collection.create();
            expect(firstModel.get("int")).toBeDefined();
            expect(firstModel.get("int")).toEqual(0);
            expect(firstModel.get("double")).toBeDefined();
            expect(firstModel.get("double")).toEqual(0);
            expect(firstModel.get("name")).toBeDefined();
            expect(firstModel.get("name")).toEqual("");
            called = true;
          });
        });
        
        waitsFor(function() {
          return called;
        }, 2000, "Creation should have been called."); 
      
        runs(function() {
          expect(true).toBeTruthy();
        });
        
      });
      
      it('should be able to request the columns', function() {
        
        var called;
        
        runs(function() {
          called = false;
          factory.create("Number", function(collection) {
            expect(collection.getColumns).toBeDefined();
            expect(collection.getColumns).toEqual(jasmine.any(Function));
            expect(collection.getColumns()).toEqual([
              {
                name: "double",
                type: "number"
              },
              {
                name: "int",
                type: "number"
              }
            ]);
            called = true;
          });
        });
        
        waitsFor(function() {
          return called;
        }, 2000, "Creation should have been called."); 
      
        runs(function() {
          expect(true).toBeTruthy();
        });
      
      });
      
    });
    
  });
}());

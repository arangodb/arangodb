/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach, jasmine */
/*global runs, waits, waitsFor */
/*global describe, it, expect, spyOn */
/*global window*/
/*global WebWorkerWrapper*/

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

  describe('Web Worker Wrapper', function() {
    
    describe('setup process', function() {
      
      it('should throw an error if no class is given', function() {
        expect(function() {
          var s = new WebWorkerWrapper();
        }).toThrow("A class has to be given.");
      });
      
      it('should throw an error if no callback is given', function() {
        expect(function() {
          function Test(){}
          var s = new WebWorkerWrapper(Test);
        }).toThrow("A callback has to be given.");
      });
      
      it('should not throw an error if mandatory information is given', function() {
        expect(function() {
          function Test(){}
          var callback = function(){},
          s = new WebWorkerWrapper(Test, callback);
        }).not.toThrow();
      });
    });
    
    describe('setup correctly', function() {
      
      it('should create an instance of the given class', function() {
        
        var created;
        
        runs(function() {
          created = false;
          function Test(){}
          var cb = function(d){
            var data = d.data;
            if (data.cmd === "construct") {
              created = data.result;
            }
          },
          worker = new WebWorkerWrapper(Test, cb);
        });
        
        waitsFor(function(){
          return created;
        }, 1000);

        runs(function() {
          expect(created).toBeTruthy();
        });
      });
      
      it('should pass parameters for constructor', function() {
        var created;
        
        runs(function() {
          created = false;
          function Test(a){
            if (a === undefined) {
              throw "Failed";
            }
          }
          var cb = function(d){
            var data = d.data;
            if (data.cmd === "construct") {
              created = data.result;
            }
          },
          worker = new WebWorkerWrapper(Test, cb, "1");
        });
        
        waitsFor(function(){
          return created;
        }, 1000);

        runs(function() {
          expect(created).toBeTruthy();
        });
      });
      
      it('should be informed if cunstruction failed', function() {
        var created, error;
        
        runs(function() {
          created = true;
          function Test(a){
            if (a === undefined) {
              throw "Failed";
            }
          }
          var cb = function(d){
            var data = d.data;
            if (data.cmd === "construct") {
              created = data.result;
              error = data.error;
            }
          },
          worker = new WebWorkerWrapper(Test, cb);
        });
        
        waitsFor(function(){
          return !created;
        }, 1000);

        runs(function() {
          expect(created).toBeFalsy();
          expect(error).toEqual("Failed");
        });
      });
    });
    
    
  });
  
}());
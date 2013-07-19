/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach, jasmine*/
/*global describe, it, expect, spyOn */
/*global DomObserverFactory, window, _ */
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

  describe('Dom Observer', function() {
    
    it('should offer a function to create a Mutation Observer', function() {
      var factory = new DomObserverFactory();
      expect(_.isFunction(factory.createObserver)).toBeTruthy();
    });
    
    it('should create a Mutation Observer Instance', function() {
      // First is Firefox, Second is Chrome and Safari
      var Observer = window.MutationObserver || window.WebKitMutationObserver,
        factory = new DomObserverFactory();
        expect(factory.createObserver(function() {})).toEqual(jasmine.any(Observer));
    });
    
    it('should propagate the callback to the MutationObserver', function() {
      var factory,
        callback = function() {};
        if (_.isFunction(window.WebKitMutationObserver)) {
          spyOn(window, "WebKitMutationObserver");
          factory = new DomObserverFactory();
          factory.createObserver(callback);
          expect(window.WebKitMutationObserver).wasCalledWith(callback);
        } else if (_.isFunction(window.MutationObserver)) {
          spyOn(window, "MutationObserver");
          factory = new DomObserverFactory();
          factory.createObserver(callback);
          expect(window.MutationObserver).wasCalledWith(callback);
        } 
    });
    
  });

}());

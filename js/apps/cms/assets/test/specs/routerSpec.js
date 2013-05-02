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

  describe('Router', function() {
    
    beforeEach(function() {
      app = app || {};
    });
    
    it('should bind itself to the app scope', function() {
      expect(app.router).toBeDefined();
    });
    
    it('should display the welcome screen initially', function() {
      var renderMock;
      
      runs(function() {
        //Navigate to somewhere, otherwise routing wont be triggered correctly.
        app.router.navigate("undef", {trigger: true});
        spyOn(app.router.welcome, "render");
        app.router.navigate("", {trigger: true});
      });
      
      waits(1000);      
      
      runs(function() {
        expect(app.router.welcome.render).toHaveBeenCalled();
        window.history.back();
        window.history.back();
      });
      
    });
    
    it('should react to routes for navigation', function() {
      
      var renderMock;
      
      runs(function() {

        renderMock = {
          render: function(){}
        };
        
        
        spyOn(app, "ContentView").andCallFake(function(options) {
          var c = options.collection;
          expect(c.url).toEqual("content/Test");
          return renderMock;
        });
        spyOn(renderMock, "render");
        

        spyOn(app.router.navi, "render");
        
        
        app.router.navigate("/collection/Test", {trigger: true});
      });
      
      waits(1000);      
      
      runs(function() {
        expect(app.ContentView).wasCalledWith({collection: jasmine.any(Backbone.Collection)});
        expect(renderMock.render).toHaveBeenCalled();
        expect(app.router.navi.render).wasCalledWith("Test");
        window.history.back();
      });
      
    });
    
  });
}());

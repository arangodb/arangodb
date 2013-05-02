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

  describe('Navigation Bar', function() {
    
    beforeEach(function() {
      app = app || {};
    });
    
    it('should bind itself to the app scope', function() {
      expect(app.NaviView).toBeDefined();
    });
    
    it('should be able to render all items', function() {
      
      var div;
      
      runs(function() {
        div = document.createElement("navi");
        div.id = "navi";
        document.body.appendChild(div);
        var view = new app.NaviView();
        view.render();
      });
      
      waits(1000);
      
      runs(function() {
        expect($("#navi ul").length).toEqual(1);
        expect($("#navi ul li").length).toEqual(2);
        
        expect($("#navi ul li#String").length).toEqual(1);
        expect($("#navi ul li#Number").length).toEqual(1);
        
        
        document.body.removeChild(div);
      });
      
    });
    
    it('should be able to activate the selected tab', function() {
      
      var div, view;
      
      runs(function() {
        div = document.createElement("navi");
        div.id = "navi";
        document.body.appendChild(div);
        view = new app.NaviView();
        
      });
      
      waits(1000);
      
      runs(function() {
        view.render("String");
      });
      
      waits(1000);
      
      runs(function() {
        expect($("#navi ul").length).toEqual(1);
        expect($("#navi ul li").length).toEqual(2);
        expect($("#navi ul li#String.active").length).toEqual(1);
        expect($("#navi ul li#Number").length).toEqual(1);
        document.body.removeChild(div);
      });
      
    });
    
    
    describe('if it is rendered', function() {
      
      var div;
      
      beforeEach(function() {
        runs(function() {
          div = document.createElement("navi");
          div.id = "navi";
          document.body.appendChild(div);
          var view = new app.NaviView();
          view.render();
        });
      
        waits(1000);
      });
      
      afterEach(function() {
        document.body.removeChild(div);
      });
      
      it('should be able to navigate to String Collection', function() {
      
        runs(function() {
          spyOn(app.router, "navigate");
        
          $("#navi ul li#String").click();
        });
      
        waitsFor(function() {
          return app.router.navigate.wasCalled;
        }, 1000, "Navigation to String should have been called.");
      
        runs(function() {
          expect(app.router.navigate).wasCalledWith("collection/String", {trigger: true});
        });
      
      });
    
      it('should be able to navigate to Number Collection', function() {
      
        runs(function() {
          spyOn(app.router, "navigate");
        
          $("#navi ul li#Number").click();
        });
      
        waitsFor(function() {
          return app.router.navigate.wasCalled;
        }, 1000, "Navigation to Number should have been called.");
      
        runs(function() {
          expect(app.router.navigate).wasCalledWith("collection/Number", {trigger: true});
        });
      
      });
      
    });

  });
}());

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

  describe('Welcome View', function() {
    
    var contentDiv;
    
    beforeEach(function() {
      app = app || {};
      contentDiv = document.createElement("div");
      contentDiv.id = "content";
      document.body.appendChild(contentDiv);
    });
    
    afterEach(function() {
      document.body.removeChild(contentDiv);
    });
    
    it('should bind itself to the app scope', function() {
      expect(app.WelcomeView).toBeDefined();
    });
    
    describe('checking display', function() {

      it('should be able to display the welcome screen', function() {
        
        runs(function() {
          var myView = new app.WelcomeView();
          myView.render();
        });
        
        waits(1000);
        
        runs(function() {
          expect($("#content #welcome").length).toEqual(1);
          expect($("#content #welcome").eq(0).text()).toEqual("Welcome");
        });
        
      });
    });
  });
}());

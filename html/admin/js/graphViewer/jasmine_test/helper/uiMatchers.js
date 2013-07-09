/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global expect, _ */

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

var uiMatchers = uiMatchers || {};

(function UIMatchers() {
  "use strict";
  
  uiMatchers.define = function(suite) {
    suite.addMatchers({
      
      toBeTag: function(name) {
        var el = this.actual;
        this.message = function() {
          return "Expected " + el.tagName.toLowerCase() + " to be a " + name; 
        };
        return el.tagName.toLowerCase() === name;
      },
      
      toBeOfClass: function(name) {
        var el = this.actual;
        this.message = function() {
          return "Expected " + el.className + " to be " + name; 
        };
        return el.className === name;
      },
      
      toBeADropdownMenu: function() {
        var div = this.actual,
          btn = div.children[0],
          list = div.children[1],
          msg = "";
        this.message = function() {
          return "Expected " + msg;
        };
        expect(div).toBeOfClass("btn-group");
        expect(btn).toBeTag("button");
        expect(btn).toBeOfClass("btn btn-inverse btn-small dropdown-toggle");
        if (btn.getAttribute("data-toggle") !== "dropdown") {
          msg = "first elements data-toggle to be dropdown";
          return false;
        }
        if (btn.children[0].tagName.toLowerCase() !== "span"
          && btn.children[0].getAttribute("class") !== "caret") {
          msg = "first element to contain a caret";
          return false;
        }

        // Check the list
        if (list.getAttribute("class") !== "dropdown-menu") {
          msg = "list element to be of class dropdown-menu";
          return false;
        }
        return true;
      },
      
      toConformToToolboxLayout: function() {
        var box = this.actual;
        expect(box).toBeTag("div");
        expect(box.id).toEqual("toolbox");
        expect(box).toBeOfClass("btn-group btn-group-vertical pull-left toolbox");
        _.each(box.children, function(btn) {
          expect(btn).toBeTag("button");
          expect(btn).toBeOfClass("btn btn-icon");
          // Correctness of buttons is checked in eventDispatcherUISpec.
        });
        return true;
      }
    });
  };
}());
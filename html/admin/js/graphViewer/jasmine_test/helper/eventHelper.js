/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, window*/

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

var helper = helper || {};

(function eventHelper() {
  "use strict";
  
  helper.simulateDragEvent = function (objectId) {
    helper.simulateMouseEvent("mousedown", objectId);
    var e1 = document.createEvent("MouseEvents"),
      e2 = document.createEvent("MouseEvents");
    e1.initMouseEvent("mousemove", true, true, window,
      0, 5, 5, 0, 0, false, false, false, false, 0, null);
    e2.initMouseEvent("mouseup", true, true, window,
      0, 10, 10, 0, 0, false, false, false, false, 0, null);
    window.dispatchEvent(e1);
    window.dispatchEvent(e2);
  };
  
  helper.simulateMouseEvent = function (type, objectId) {
    var evt = document.createEvent("MouseEvents"),
      testee = document.getElementById(objectId);
    evt.initMouseEvent(type, true, true, window,
      0, 0, 0, 0, 0, false, false, false, false, 0, null);
    testee.dispatchEvent(evt);
  };
  
  helper.simulateScrollUpMouseEvent = function (objectId) {
    var evt = document.createEvent("MouseEvents"),
      testee = document.getElementById(objectId);
    evt.initMouseEvent("DOMMouseScroll", true, true, window,
      -10, 0, 0, 0, 0, false, false, false, false, 0, null);
    testee.dispatchEvent(evt);
  };
  
  helper.simulateScrollDownMouseEvent = function (objectId) {
    var evt = document.createEvent("MouseEvents"),
      testee = document.getElementById(objectId);
    evt.initMouseEvent("DOMMouseScroll", true, true, window,
      10, 0, 0, 0, 0, false, false, false, false, 0, null);
    testee.dispatchEvent(evt);
  };
  
}());
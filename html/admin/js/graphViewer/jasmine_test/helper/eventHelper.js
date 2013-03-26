/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global document, window*/
var helper = helper || {};

(function eventHelper() {
  "use strict";
  
  helper.simulateMouseEvent = function (type, objectId) {
    var evt = document.createEvent("MouseEvents"),
    testee = document.getElementById(objectId); 
    evt.initMouseEvent(type, true, true, window,
      0, 0, 0, 0, 0, false, false, false, false, 0, null);
    testee.dispatchEvent(evt);
  };
  
}());
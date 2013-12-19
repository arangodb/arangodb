/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global EJS*/

(function() {
  "use strict";
  var FakeTE = function() {
    var prefix = "base/frontend/js/templates/",
      exports = {};
    exports.createTemplate = function(path) {
      var param = {
        url: prefix + path
      };
      return new EJS(param);
    };
    return exports;
  };
  window.templateEngine = new FakeTE();
}());

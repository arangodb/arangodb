/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global EJS*/

(function() {
  "use strict";
  var FakeTE = function(prefix) {
    var exports = {};
    exports.createTemplate = function(path) {
      var param = {
        url: prefix + path
      };
      return new EJS(param);
    };
    return exports;
  };
  window.templateEngine = new FakeTE("base/frontend/js/templates/");
  window.plannerTemplateEngine = new FakeTE("base/plannerFrontend/js/templates/");
}());

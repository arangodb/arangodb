/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global EJS, window*/
(function() {
  "use strict";
  var TemplateEngine = function(prefix) {
    prefix = prefix || "";
    var exports = {};
    exports.createTemplate = function(path) {
      var param = {
        url: prefix + path
      };
      return new EJS(param);
    };
    return exports;
  };
  window.templateEngine = new TemplateEngine("js/templates/"); 
  window.plannerTemplateEngine = new TemplateEngine("js/plannerTemplates/"); 
}());

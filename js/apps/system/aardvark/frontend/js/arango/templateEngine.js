/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global EJS, window, _, $*/
(function() {
  "use strict";
  var TemplateEngine = function() {
    var exports = {};
    exports.createTemplate = function(id) {
      var template = $("#" + id.replace(".", "\\.")).html();
      return {
        render: function(params) {
          return _.template(template, params);
        }
      };
    };
    return exports;
  };
  window.templateEngine = new TemplateEngine(); 
}());

/*jshint unused: false */
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

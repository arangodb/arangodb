/*jshint unused: false */
/*global EJS, window, _, $*/
(function() {
  "use strict";
  // For tests the templates are loaded some where else.
  // We need to use a different engine there.
  if (!window.hasOwnProperty("TEST_BUILD")) {
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
  }
}());

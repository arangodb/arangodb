/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global window, _, $, document*/
(function() {
  "use strict";
  var TemplateEngine = function() {
    var exports = {};
    exports.createTemplate = function(id) {
      var ref = "#" + id.replace(".", "\\.");
      var template;
      if ($(ref).length === 0) {
        var path = "../../base/frontend/js/templates/" + id;
        $.ajax({
          url: path,
          type: "GET",
          async: false
        }).done(function(script) {
          $(document.head).append(script);
        });
      }
      template = $(ref).html();
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

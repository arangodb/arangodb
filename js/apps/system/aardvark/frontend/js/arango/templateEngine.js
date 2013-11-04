(function() {
  "use strict";
  var TemplateEngine = function(prefix) {
    prefix = prefix || "";
    var exports = {};
    exports.createTemplate = function(path) {
      console.log(path);
      var param = {
        url: prefix + path
      };
      return new EJS(param);
    };
    return exports;
  };
  window.templateEngine = new TemplateEngine("js/templates/"); 
}());

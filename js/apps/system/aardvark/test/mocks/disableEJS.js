(function() {
  var FakeTE = function() {
    var prefix = "base/frontend/js/templates/";
    var exports = {};
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

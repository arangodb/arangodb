(function() {
  var FakeTE = function() {
    var prefix = "base/frontend/js/templates/";
    var exports = {};
    exports.createTemplate = function(path) {
      _.each(window.__karma__.files, function(v,k) {
        if (k.indexOf(path, k.length - path.length) !== -1) {
          console.log(v);
        }
      });
      var param = {
        url: prefix + path
      };
      return new EJS(param);
    };
    return exports;
  };
  window.templateEngine = new FakeTE();
}());

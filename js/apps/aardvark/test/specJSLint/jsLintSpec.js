describe('JSLint', function () {
  var options = {},
      lint = /^\/specJSLint|.jsLintSpec\.js$/;

  function get(path) {
    path = path + "?" + new Date().getTime();

    var xhr;
    try {
      xhr = new jasmine.XmlHttpRequest();
      xhr.open("GET", path, false);
      xhr.send(null);
    } catch (e) {
      throw new Error("couldn't fetch " + path + ": " + e);
    }
    if (xhr.status < 200 || xhr.status > 299) {
      throw new Error("Could not load '" + path + "'.");
    }

    return xhr.responseText;
  }

  describe('checking codeFiles', function() {
   var files = /^(.*lib\/.*)|(.*Spec\.js)$/;
    
    _.each(document.getElementsByTagName('script'), function (element) {
      var script = element.getAttribute('src');
      if (files.test(script)) {
        return;
      }
      it(script, function () {
        var self = this;
        var source = get(script);
        var result = JSLINT(source, options);
        _.each(JSLINT.errors, function (error) {
          self.addMatcherResult(new jasmine.ExpectationResult({
            passed: false,
            message: "line " + error.line + ' - ' + error.reason + ' - ' + error.evidence
          }));
        });
        expect(true).toBe(true); // force spec to show up if there are no errors
      });
    });
  });

  describe('checking specFiles', function() {
    var files = /^\/spec*|.*Spec\.js$/;
    
    _.each(document.getElementsByTagName('script'), function (element) {
      var script = element.getAttribute('src');
      if (!files.test(script) || lint.test(script)) {
        return;
      }
      it(script, function () {
        var self = this;
        var source = get(script);
        var result = JSLINT(source, options);
        _.each(JSLINT.errors, function (error) {
          self.addMatcherResult(new jasmine.ExpectationResult({
            passed: false,
            message: "line " + error.line + ' - ' + error.reason + ' - ' + error.evidence
          }));
        });
        expect(true).toBe(true); // force spec to show up if there are no errors
      });
    });
  });
});
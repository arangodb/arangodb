'use strict';


var path = require('path');
var fs   = require('fs');


suite('Issues', function () {
  var issues = path.resolve(__dirname, 'issues');

  fs.readdirSync(issues).forEach(function (file) {
    if ('.js' === path.extname(file)) {
      require(path.resolve(issues, file));
    }
  });
});

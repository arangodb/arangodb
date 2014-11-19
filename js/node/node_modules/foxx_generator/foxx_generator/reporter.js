(function () {
  'use strict';
  var report,
    developmentMode = false;

  report = function () {
    if (developmentMode) {
      require('console').log.apply(this, arguments);
    }
  };

  exports.report = report;
}());

(function () {
  'use strict';

  var Documentation = function (applicationContext) {
    this.summary = '';
    this.notes = '';

    if (applicationContext.comments.length > 0) {
      do {
        this.summary = applicationContext.comments.shift();
      } while (this.summary === '');
      this.notes = applicationContext.comments.join('\n');
    }

    applicationContext.clearComments();
  };

  exports.Documentation = Documentation;
}());

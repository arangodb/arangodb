/*jshint browserify: true */
'use strict';
function AqlError(message) {
  this.message = message;
  var err = new Error(message);
  err.name = this.name;
  if (err.hasOwnProperty('stack')) this.stack = err.stack;
  if (err.hasOwnProperty('description')) this.description = err.description;
  if (err.hasOwnProperty('lineNumber')) this.lineNumber = err.lineNumber;
  if (err.hasOwnProperty('fileName')) this.fileName = err.fileName;
}
AqlError.prototype = new Error();
AqlError.prototype.constructor = AqlError;
AqlError.prototype.name = 'AqlError';

exports.AqlError = AqlError;

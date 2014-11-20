(function () {
  'use strict';
  var VertexNotFound;

  VertexNotFound = function () {
    this.name = 'VertexNotFound';
    this.message = 'The vertex could not be found';
  };
  VertexNotFound.prototype = Error.prototype;

  exports.VertexNotFound = VertexNotFound;
}());

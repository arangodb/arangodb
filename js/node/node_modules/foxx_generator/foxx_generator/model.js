(function () {
  'use strict';
  var Foxx = require('org/arangodb/foxx'),
    Model;

  Model = Foxx.Model.extend({
    forClient: function () {
      var properties = Foxx.Model.prototype.forClient.call(this);

      return {
        properties: properties,
        links: []
      };
    }
  });

  exports.Model = Model;
}());

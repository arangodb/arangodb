(function () {
  'use strict';
  var Strategy,
    _ = require('underscore'),
    extend = require('org/arangodb/extend').extend;

  Strategy = function () {
  };

  _.extend(Strategy.prototype, {
    executable: function (type, from, to, cardinality) {
      return type === this.type && from === this.from && to === this.to && cardinality === this.cardinality;
    },

    execute: function () {
      require('console').log('Nothing to do for strategy type "%s" from "%s" to "%s" with cardinality "%s"',
                             this.type,
                             this.from,
                             this.to,
                             this.cardinality);
    }
  });

  Strategy.extend = extend;

  exports.Strategy = Strategy;
}());

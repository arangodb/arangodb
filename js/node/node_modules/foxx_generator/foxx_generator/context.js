(function () {
  'use strict';
  var Context,
    _ = require('underscore'),
    extend = require('org/arangodb/extend').extend;

  /*jshint maxlen: 200 */
  Context = function (from, to, relation) {
    from.relations.push(relation);

    this.from = from;
    this.to = to;
    this.relation = relation;
    this.strategy = _.find(this.strategies, function (maybeStrategy) {
      return maybeStrategy.executable(relation.type, from.type, to.type, relation.cardinality);
    });

    if (_.isUndefined(this.strategy)) {
      require('console').log('Couldn\'t find a strategy for semantic %s from %s to %s (%s)', relation.type, from.type, to.type, relation.cardinality);
      throw 'Could not find strategy';
    }
  };
  /*jshint maxlen: 100 */

  _.extend(Context.prototype, {
    execute: function (controller, graph) {
      this.strategy.execute(controller, graph, this.relation, this.from, this.to);
    }
  });

  Context.extend = extend;

  exports.Context = Context;
}());

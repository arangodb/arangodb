(function () {
  'use strict';
  var extend = require('org/arangodb/extend').extend,
    ConditionNotFulfilled = require('./condition_not_fulfilled').ConditionNotFulfilled,
    _ = require('underscore'),
    Transition;

  Transition = function (graph, controller) {
    this.graph = graph;
    this.controller = controller;
  };

  _.extend(Transition.prototype, {
    extendEdgeDefinitions: function(from, to) {
      var edgeCollectionName = this.collectionBaseName + '_' + from.name + '_' + to.name;
      return this.graph.extendEdgeDefinitions(edgeCollectionName, from, to);
    },

    wrappedCondition: function () {
      var condition = this.condition;

      return function (req) {
        if (!condition(req)) {
          throw new ConditionNotFulfilled('Condition was not fulfilled');
        }
      };
    },

    createContext: function(from, to) {
      var context = new this.Context(from, to, {
        name: this.relationName,
        edgeCollectionName: this.extendEdgeDefinitions(from, to),
        cardinality: this.cardinality,
        type: this.type,
        parameters: this.parameters,
        summary: this.summary,
        notes: this.notes,
        condition: this.wrappedCondition(),
        precondition: this.precondition,
        to: to
      });

      return context;
    },

    apply: function (from, to) {
      this.createContext(from, to).execute(this.controller, this.graph);
    }
  });

  Transition.extend = extend;

  exports.Transition = Transition;
}());

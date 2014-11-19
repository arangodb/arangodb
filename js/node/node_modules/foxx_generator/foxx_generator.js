(function () {
  'use strict';
  var Foxx = require('org/arangodb/foxx'),
    _ = require('underscore'),
    Graph = require('./foxx_generator/graph').Graph,
    Generator,
    StateFactory = require('./foxx_generator/state_factory').StateFactory,
    TransitionFactory = require('./foxx_generator/transition_factory').TransitionFactory,
    configureStates = require('./foxx_generator/configure_states').configureStates,
    mediaTypes;

  mediaTypes = {
    'application/vnd.siren+json': require('./foxx_generator/siren').mediaType
  };

  Generator = function (name, options) {
    var applicationContext = options.applicationContext,
      graph = new Graph(name, applicationContext),
      strategies = mediaTypes[options.mediaType].strategies;

    this.controller = new Foxx.Controller(applicationContext, options);

    this.states = {};
    this.transitions = [];

    this.stateFactory = new StateFactory(graph, this.transitions, this.states);
    this.transitionFactory = new TransitionFactory(applicationContext, graph, this.controller, strategies);
  };

  _.extend(Generator.prototype, {
    addStartState: function (opts) {
      var name = '',
        options = _.defaults({ type: 'start', controller: this.controller }, opts);

      this.states[name] = this.stateFactory.create(name, options);
    },

    addState: function (name, opts) {
      this.states[name] = this.stateFactory.create(name, opts);
    },

    defineTransition: function (name, opts) {
      this.transitions[name] = this.transitionFactory.create(name, opts);
    },

    generate: function () {
      configureStates(this.states);
      _.each(this.states, function (state) { state.prepareTransitions(this.transitions, this.states); }, this);
      _.each(this.states, function (state) { state.applyTransitions(); }, this);
    }
  });

  exports.Generator = Generator;
}());

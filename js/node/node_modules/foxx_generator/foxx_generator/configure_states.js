(function () {
  'use strict';
  var _ = require('underscore'),
    R = require('ramda'),
    Repository = require('./repository_with_graph').RepositoryWithGraph,
    Model = require('./model').Model,
    configureStates,
    typeIs,
    determineSuperstate,
    prepareStartState,
    prepareServiceState,
    determinePrefix,
    prependPrefix,
    appendIdPlaceholder,
    isParameterized,
    hasUrlTemplate,
    appendIdPlaceholderForParameterizedState,
    determineUrlTemplate,
    determineUrlTemplateIfNotDetermined,
    prepareEntityState,
    prepareRepositoryState,
    copyInfoFromRepositoryState;

  typeIs = R.curry(function (type, state) {
    return state.type === type;
  });

  determineSuperstate = R.curry(function (states, state) {
    if (state.superstate) {
      state.superstate = states[state.superstate];
    }
  });

  prepareStartState = R.curry(R.func('setAsStart'));
  prepareServiceState = R.curry(R.func('addService'));

  determinePrefix = function (state) {
    var prefix = '/';

    if (state.superstate) {
      // First determine the entire chain
      determineUrlTemplateIfNotDetermined(state.superstate);
      prefix = state.superstate.urlTemplate + '/';
    }

    return prefix;
  };

  prependPrefix = function (state) {
    state.urlTemplate = R.concat(determinePrefix(state), state.name);
    return state;
  };

  appendIdPlaceholder = function(state) {
    state.urlTemplate = R.concat(state.urlTemplate, '/:id');
    return state;
  };

  isParameterized = R.prop('parameterized');
  hasUrlTemplate = R.prop('urlTemplate');
  appendIdPlaceholderForParameterizedState = R.cond(isParameterized, appendIdPlaceholder, R.identity);
  determineUrlTemplate = R.pipe(prependPrefix, appendIdPlaceholderForParameterizedState);
  determineUrlTemplateIfNotDetermined = R.cond(hasUrlTemplate, R.identity, determineUrlTemplate);

  prepareEntityState = R.curry(function (states, entity) {
    var repositoryState = states[entity.options.containedIn];
    entity.repositoryState = repositoryState;
    entity.addModel(Model);
  });

  prepareRepositoryState = R.curry(function (states, repository) {
    var entityState = states[repository.options.contains];
    repository.entityState = entityState;
    repository.model = entityState.model;
    repository.addRepository(Repository);
  });

  copyInfoFromRepositoryState = function (entity) {
    var repositoryState = entity.repositoryState,
      repository = repositoryState.repository;
    entity.collectionName = repositoryState.collectionName;
    entity.collection = repositoryState.collection;
    entity.repository = repositoryState.repository;
    repository.relations = entity.relations;
  };

  configureStates = function (states) {
    var entities = _.filter(states, typeIs('entity')),
      repositories = _.filter(states, typeIs('repository')),
      services = _.filter(states, typeIs('service')),
      starts = _.filter(states, typeIs('start'));

    _.each(states, determineSuperstate(states));
    _.each(states, determineUrlTemplateIfNotDetermined);
    _.each(starts, prepareStartState);
    _.each(services, prepareServiceState);
    _.each(entities, prepareEntityState(states));
    _.each(repositories, prepareRepositoryState(states));
    _.each(entities, copyInfoFromRepositoryState);
  };

  exports.configureStates = configureStates;
}());

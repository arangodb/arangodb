(function () {
  'use strict';
  var Foxx = require('org/arangodb/foxx'),
    ConditionNotFulfilled = require('./condition_not_fulfilled').ConditionNotFulfilled,
    VertexNotFound = require('./vertex_not_found').VertexNotFound,
    wrapServiceAction = require('./wrap_service_action').wrapServiceAction,
    constructBodyParams,
    joi = require('joi'),
    constructRoute;

  constructBodyParams = function (relation) {
    return Foxx.Model.extend({ schema: relation.parameters });
  };

  constructRoute = function (opts) {
    var route,
      controller = opts.controller,
      // from = opts.from,
      // graph = opts.graph,
      to = opts.to,
      verb,
      url = opts.url || to.urlTemplate,
      action,
      relation = opts.relation;

    if (to.type === 'service') {
      verb = to.verb;
      action = wrapServiceAction(to);
    } else {
      verb = opts.verb;
      action = opts.action;
    }

    route = controller[verb](url, action)
      .errorResponse(VertexNotFound, 404, 'The vertex could not be found')
      .errorResponse(ConditionNotFulfilled, 403, 'The condition could not be fulfilled')
      .onlyIf(relation.condition)
      .summary(relation.summary)
      .notes(relation.notes);

    if (url.indexOf(':') > 0) {
      route.pathParam('id', joi.string().description('ID of the entity'));
    }

    if (opts.body) {
      route.bodyParam(opts.body.name, 'TODO', constructBodyParams(relation));
    }
  };

  exports.constructRoute = constructRoute;
}());


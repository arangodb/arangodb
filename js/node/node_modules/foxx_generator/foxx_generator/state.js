(function () {
  'use strict';

  var stateTypes = ['entity', 'repository', 'service', 'start'],
    extend = require('org/arangodb/extend').extend,
    _ = require('underscore'),
    constructFields,
    State;

  constructFields = function (relation) {
    return _.map(relation.parameters, function (joi, name) {
      var fieldDescription = { name: name, type: joi._type };

      if (!_.isNull(joi._description)) {
        fieldDescription.description = joi._description;
      }

      if (!_.isUndefined(joi._flags.default)) {
        fieldDescription.value = joi._flags.default;
      }

      return fieldDescription;
    });
  };

  State = function (name, graph, options) {
    this.name = name;
    this.graph = graph;
    this.options = options;
    this.parameterized = this.options.parameterized;
    this.superstate = this.options.superstate;
    this.type = this.options.type;

    if (!_.contains(stateTypes, this.type)) {
      require('console').log('Unknown state type "' + options.type + '"');
      throw 'Unknown State Type';
    }

    this.links = [];
    this.actions = [];
    this.childLinks = [];
    this.relations = [];
  };

  _.extend(State.prototype, {
    prepareTransitions: function (definitions, states) {
      this.transitions = _.map(this.options.transitions, function (transitionDescription) {
        var transition = definitions[transitionDescription.via],
          to = states[transitionDescription.to];

        return {
          transition: transition,
          to: to
        };
      }, this);
    },

    applyTransitions: function () {
      _.each(this.transitions, function (transitionDescription) {
        transitionDescription.transition.apply(this, transitionDescription.to);
      }, this);
    },

    properties: function () {
      return {};
    },

    setAsStart: function () {
      var that = this;

      this.options.controller.get('/', function (req, res) {
        res.json({
          properties: {},
          links: that.filteredLinks(req),
          actions: that.filteredActions(req)
        });
      }).summary('Billboard URL')
        .notes('This is the starting point for using the API');
    },

    addRepository: function (Repository) {
      this.collection = this.graph.addVertexCollection(this.name);
      this.collectionName = this.collection.name();

      this.repository = new Repository(this.collection, {
        model: this.model,
        graph: this.graph
      });
    },

    addModel: function (Model) {
      this.model = Model.extend({
        schema: _.extend(this.options.attributes, { links: { type: 'object' } })
      }, {
        state: this,
      });
    },

    addService: function () {
      this.action = this.options.action;
      this.verb = this.options.verb.toLowerCase();
    },

    urlForEntity: function (selector) {
      if (!this.parameterized) {
        throw 'This is not a paremeterized state';
      }

      return this.urlTemplate.replace(':id', selector);
    },

    urlForRelation: function (relation) {
      return this.urlTemplate + '/links/' + relation.name;
    },

    entities: function () {
      var entities = [];

      if (this.type === 'repository') {
        entities = _.map(this.repository.all(), function (entity) {
          var result = entity.forClient();

          _.each(this.childLinks, function (link) {
            result.links.push({
              rel: link.rel,
              href: link.target.urlForEntity(entity.get('_key')),
              title: link.title
            });
          });
          return result;
        }, this);
      }

      return entities;
    },

    filteredLinks: function (req) {
      return _.filter(this.links, function (link) {
        return link.precondition(req);
      });
    },

    filteredActions: function (req) {
      return _.filter(this.actions, function (action) {
        return action.precondition(req);
      });
    },

    addLink: function (rel, href, title, precondition) {
      this.links.push({
        precondition: precondition,
        rel: rel,
        href: href,
        title: title
      });
    },

    addLinkViaTransitionTo: function (relation, to) {
      this.addLink([relation.name], to.urlTemplate, relation.summary, relation.precondition);
    },

    addLinkToEntities: function (relation, to) {
      var rel = relation.name,
        href = to.urlTemplate,
        title = relation.summary,
        target = to;

      this.childLinks.push({
        rel: rel,
        href: href,
        title: title,
        target: target
      });
    },

    addAction: function (name, method, href, title, fields, precondition) {
      this.actions.push({
        precondition: precondition,
        name: name,
        // class: ?,
        method: method,
        href: href,
        title: title,
        type: 'application/json',
        fields: fields
      });
    },

    addActionWithMethodForRelation: function (method, relation) {
      var name = relation.name,
        urlTemplate = this.urlTemplate,
        summary = relation.summary,
        fields = constructFields(relation),
        precondition = relation.precondition;

      this.addAction(name, method, urlTemplate, summary, fields, precondition);
    }
  });

  State.extend = extend;

  exports.State = State;
}());

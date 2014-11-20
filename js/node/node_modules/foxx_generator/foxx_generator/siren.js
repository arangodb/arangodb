(function () {
  'use strict';
  var RelationRepository = require('./relation_repository').RelationRepository,
    Strategy = require('./strategy').Strategy,
    constructRoute = require('./construct_route').constructRoute,
    ModifyAnEntity,
    AddEntityToRepository,
    ConnectRepoWithEntity,
    ConnectStartWithRepository,
    ConnectToService,
    DisconnectTwoEntities,
    DisconnectTwoEntitiesToMany,
    ConnectEntityToService,
    ConnectTwoEntities,
    ConnectTwoEntitiesToMany,
    FollowToEntity,
    FollowToEntityToMany,
    FollowFromRepositoryToService;

  ConnectEntityToService = Strategy.extend({
    type: 'follow',
    from: 'entity',
    to: 'service',
    cardinality: 'one',

    execute: function (controller, graph, relation, from, to) {
      constructRoute({
        controller: controller,
        graph: graph,
        from: from,
        to: to,
        relation: relation
      });
    }
  });

  ModifyAnEntity = Strategy.extend({
    type: 'modify',
    from: 'entity',
    to: 'entity',
    cardinality: 'one',

    execute: function (controller, graph, relation, from, to) {
      constructRoute({
        controller: controller,
        graph: graph,
        verb: 'patch',
        url: from.urlTemplate,
        action: function (req, res) {
          var id = req.params('id'),
          patch = req.params(from.name),
          result;

          from.repository.updateById(id, patch.forDB());
          result = from.repository.byIdWithNeighbors(id);

          res.json(result.forClient());
        },
        from: from,
        to: to,
        relation: relation,
        body: from
      });
    }
  });

  ConnectTwoEntities = Strategy.extend({
    type: 'connect',
    from: 'entity',
    to: 'entity',
    cardinality: 'one',

    execute: function (controller, graph, relation, from, to) {
      var relationRepository = new RelationRepository(from, to, relation, graph);

      constructRoute({
        controller: controller,
        graph: graph,
        verb: 'post',
        url: from.urlForRelation(relation),
        action: function (req, res) {
          relationRepository.replaceRelation(req.params('id'), req.body()[relation.name]);
          res.status(204);
        },
        from: from,
        to: to,
        relation: relation
      });
    }
  });

  ConnectTwoEntitiesToMany = Strategy.extend({
    type: 'connect',
    from: 'entity',
    to: 'entity',
    cardinality: 'many',

    execute: function (controller, graph, relation, from, to) {
      var relationRepository = new RelationRepository(from, to, relation, graph);

      constructRoute({
        controller: controller,
        graph: graph,
        verb: 'post',
        url: from.urlForRelation(relation),
        action: function (req, res) {
          relationRepository.addRelations(req.params('id'), req.body()[relation.name]);
          res.status(204);
        },
        from: from,
        to: to,
        relation: relation
      });
    }
  });

  DisconnectTwoEntities = Strategy.extend({
    type: 'disconnect',
    from: 'entity',
    to: 'entity',
    cardinality: 'one',

    execute: function (controller, graph, relation, from, to) {
      var relationRepository = new RelationRepository(from, to, relation, graph);

      constructRoute({
        controller: controller,
        graph: graph,
        verb: 'delete',
        url: from.urlForRelation(relation),
        action: function (req, res) {
          relationRepository.deleteRelation(req.params('id'));
          res.status(204);
        },
        from: from,
        to: to,
        relation: relation
      });
    }
  });

  DisconnectTwoEntitiesToMany = Strategy.extend({
    type: 'disconnect',
    from: 'entity',
    to: 'entity',
    cardinality: 'many'
  });


  FollowToEntity = Strategy.extend({
    type: 'follow',
    from: 'entity',
    to: 'entity',
    cardinality: 'one'
  });

  FollowToEntityToMany = Strategy.extend({
    type: 'follow',
    from: 'entity',
    to: 'entity',
    cardinality: 'many'
  });

  AddEntityToRepository = Strategy.extend({
    type: 'connect',
    from: 'repository',
    to: 'entity',
    cardinality: 'one',

    execute: function (controller, graph, relation, from, to) {
      from.addActionWithMethodForRelation('POST', relation);

      constructRoute({
        controller: controller,
        graph: graph,
        verb: 'post',
        url: from.urlTemplate,
        action: function (req, res) {
          var data = {},
          model = req.params(to.name);

          data[to.name] = from.repository.save(model).forClient();
          res.status(201);
          res.json(data);
        },
        from: from,
        to: to,
        relation: relation,
        body: to
      });
    }
  });

  ConnectRepoWithEntity = Strategy.extend({
    type: 'follow',
    from: 'repository',
    to: 'entity',
    cardinality: 'one',

    execute: function (controller, graph, relation, from, to) {
      constructRoute({
        controller: controller,
        graph: graph,
        verb: 'get',
        action: function (req, res) {
          var id = req.params('id'),
          entry = from.repository.byIdWithNeighbors(id);

          res.json(entry.forClient());
        },
        from: from,
        to: to,
        relation: relation
      });

      from.addLinkToEntities(relation, to);
    }
  });

  ConnectStartWithRepository = Strategy.extend({
    type: 'follow',
    from: 'start',
    to: 'repository',
    cardinality: 'one',

    execute: function (controller, graph, relation, from, to) {
      from.addLinkViaTransitionTo(relation, to);

      constructRoute({
        controller: controller,
        graph: graph,
        verb: 'get',
        action: function (req, res) {
          res.json({
            properties: to.properties(),
            entities: to.entities(),
            links: to.filteredLinks(req),
            actions: to.filteredActions(req)
          });
        },
        from: from,
        to: to,
        relation: relation
      });
    }
  });

  ConnectToService = Strategy.extend({
    type: 'follow',
    from: 'start',
    to: 'service',
    cardinality: 'one',

    execute: function (controller, graph, relation, from, to) {
      from.addLinkViaTransitionTo(relation, to);

      constructRoute({
        controller: controller,
        graph: graph,
        from: from,
        to: to,
        relation: relation,
        body: to
      });
    }
  });

  FollowFromRepositoryToService = Strategy.extend({
    type: 'follow',
    from: 'repository',
    to: 'service',
    cardinality: 'one',

    execute: function (controller, graph, relation, from, to) {
      from.addLinkViaTransitionTo(relation, to);

      constructRoute({
        controller: controller,
        graph: graph,
        from: from,
        to: to,
        relation: relation
      });
    }
  });

  exports.mediaType = {
    strategies: [
      new ModifyAnEntity(),
      new ConnectTwoEntities(),
      new DisconnectTwoEntities(),
      new DisconnectTwoEntitiesToMany(),
      new FollowToEntity(),
      new FollowToEntityToMany(),
      new AddEntityToRepository(),
      new ConnectRepoWithEntity(),
      new ConnectEntityToService(),
      new ConnectToService(),
      new ConnectTwoEntitiesToMany(),
      new ConnectStartWithRepository(),
      new FollowFromRepositoryToService()
    ]
  };
}());

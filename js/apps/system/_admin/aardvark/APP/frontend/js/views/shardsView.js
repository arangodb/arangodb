/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window, _ */
(function () {
  'use strict';

  window.ShardsView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('shardsView.ejs'),
    interval: 10000,
    knownServers: [],

    events: {
      'click #shardsContent .shardLeader span': 'moveShard',
      'click #shardsContent .shardFollowers span': 'moveShardFollowers',
      'click #rebalanceShards': 'rebalanceShards'
    },

    initialize: function (options) {
      var self = this;

      self.dbServers = options.dbServers;
      clearInterval(this.intervalFunction);

      if (window.App.isCluster) {
        this.updateServerTime();

        // start polling with interval
        this.intervalFunction = window.setInterval(function () {
          if (window.location.hash === '#shards') {
            self.render(false);
          }
        }, this.interval);
      }
    },

    render: function (navi) {
      var self = this;

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/shardDistribution'),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: function (data) {
          var collsAvailable = false;
          var collName;
          self.shardDistribution = data.results;

          _.each(data.results, function (ignore, name) {
            collName = name.substring(0, 1);
            if (collName !== '_' && name !== 'error' && name !== 'code') {
              collsAvailable = true;
            }
          });

          if (collsAvailable) {
            self.continueRender(data.results);
          } else {
            arangoHelper.renderEmpty('No collections and no shards available');
          }
        },
        error: function (data) {
          if (data.readyState !== 0) {
            arangoHelper.arangoError('Cluster', 'Could not fetch sharding information.');
          }
        }
      });

      if (navi !== false) {
        arangoHelper.buildNodesSubNav('Shards');
      }
    },

    moveShardFollowers: function (e) {
      var from = $(e.currentTarget).html();
      this.moveShard(e, from);
    },

    moveShard: function (e, from) {
      var self = this;
      var fromServer, collectionName, shardName, leader;
      var dbName = window.App.currentDB.get('name');
      collectionName = $(e.currentTarget).parent().parent().attr('collection');
      shardName = $(e.currentTarget).parent().parent().attr('shard');

      if (!from) {
        fromServer = $(e.currentTarget).parent().parent().attr('leader');
      } else {
        leader = $(e.currentTarget).parent().parent().attr('leader');
        fromServer = from;
      }

      var buttons = [];
      var tableContent = [];

      var obj = {};
      var array = [];

      self.dbServers[0].each(function (db) {
        if (db.get('name') !== fromServer) {
          obj[db.get('name')] = {
            value: db.get('name'),
            label: db.get('name')
          };
        }
      });

      _.each(self.shardDistribution[collectionName].Plan[shardName].followers, function (follower) {
        delete obj[follower];
      });

      if (from) {
        delete obj[leader];
      }

      _.each(obj, function (value) {
        array.push(value);
      });

      array = array.reverse();

      if (array.length === 0) {
        arangoHelper.arangoMessage('Shards', 'No database server for moving the shard is available.');
        return;
      }

      tableContent.push(
        window.modalView.createSelectEntry(
          'toDBServer',
          'Destination',
          undefined,
          // this.users !== null ? this.users.whoAmI() : 'root',
          'Please select the target databse server. The selected database ' +
          'server will be the new leader of the shard.',
          array
        )
      );

      buttons.push(
        window.modalView.createSuccessButton(
          'Move',
          this.confirmMoveShards.bind(this, dbName, collectionName, shardName, fromServer)
        )
      );

      window.modalView.show(
        'modalTable.ejs',
        'Move shard: ' + shardName,
        buttons,
        tableContent
      );
    },

    confirmMoveShards: function (dbName, collectionName, shardName, fromServer) {
      var self = this;
      var toServer = $('#toDBServer').val();

      var data = {
        database: dbName,
        collection: collectionName,
        shard: shardName,
        fromServer: fromServer,
        toServer: toServer
      };

      $.ajax({
        type: 'POST',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/moveShard'),
        contentType: 'application/json',
        processData: false,
        data: JSON.stringify(data),
        async: true,
        success: function (data) {
          if (data === true) {
            window.setTimeout(function () {
              self.render(false);
            }, 1500);
            arangoHelper.arangoNotification('Shard ' + shardName + ' will be moved to ' + toServer + '.');
          }
        },
        error: function () {
          arangoHelper.arangoNotification('Shard ' + shardName + ' could not be moved to ' + toServer + '.');
        }
      });

      window.modalView.hide();
    },

    rebalanceShards: function () {
      var self = this;

      $.ajax({
        type: 'POST',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/rebalanceShards'),
        contentType: 'application/json',
        processData: false,
        data: JSON.stringify({}),
        async: true,
        success: function (data) {
          if (data === true) {
            window.setTimeout(function () {
              self.render(false);
            }, 1500);
            arangoHelper.arangoNotification('Started rebalance process.');
          }
        },
        error: function () {
          arangoHelper.arangoNotification('Could not start rebalance process.');
        }
      });

      window.modalView.hide();
    },

    continueRender: function (collections) {
      delete collections.code;
      delete collections.error;

      this.$el.html(this.template.render({
        collections: collections
      }));
    },

    updateServerTime: function () {
      this.serverTime = new Date().getTime();
    }

  });
}());

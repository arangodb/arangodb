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
    pending: false,
    visibleCollection: null,

    events: {
      'click #shardsContent .shardLeader span': 'moveShard',
      'click #shardsContent .shardFollowers span': 'moveShardFollowers',
      'click #rebalanceShards': 'rebalanceShards',
      'click .sectionHeader': 'toggleSections'
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

    remove: function () {
      clearInterval(this.intervalFunction);
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    renderArrows: function (e) {
      $('#shardsContent .fa-arrow-down').removeClass('fa-arrow-down').addClass('fa-arrow-right');
      $(e.currentTarget).find('.fa-arrow-right').removeClass('fa-arrow-right').addClass('fa-arrow-down');
    },

    toggleSections: function (e) {
      var colName = $(e.currentTarget).parent().attr('id');
      this.visibleCollection = colName;
      $('.sectionShardContent').hide();
      $(e.currentTarget).next().show();
      this.renderArrows(e);

      this.getShardDetails(colName);
    },

    renderShardDetail: function (collection, data) {
      var inSync = 0;
      var total = 0;

      var percentify = function (value) {
        if (value > 100) {
          // do not exceed 100%, because this looks unintuitive. however, it is possible
          // to get above 100% here because our method simply divides counts, and there
          // can be more documents on the follower than on the leader during catch-up
          value = 100;
        }
        return value.toFixed(1) + '%';
      };

      _.each(data.results[collection].Plan, function (value, shard) {
        var shardProgress = '';
        var followersSyncing = '';
        var working = '';

        if (value.progress) {
          if (value.progress.hasOwnProperty('followersSyncing') && 
              value.progress.followersSyncing > 0) {
            // number of followers currently running the synchronization for the shard
            followersSyncing = '<span>' + arangoHelper.escapeHtml(value.progress.followersSyncing) + ' follower';
            if (value.progress.followersSyncing > 1) {
              // pluralize
              followersSyncing += 's';
            }
            followersSyncing += ' syncing...</span> ';
            working = ' <i class="fa fa-circle-o-notch fa-spin fa-fw"></i>';
          }

          if (value.progress.hasOwnProperty('followerPercent') &&
              typeof value.progress.followerPercent === 'number') {
            shardProgress = percentify(value.progress.followerPercent);
          } else if (value.progress.current !== 0) {
            shardProgress = percentify(value.progress.current / value.progress.total * 100);
          }
          if (shardProgress === '' || followersSyncing === '') {
            shardProgress = 'waiting for slot...';
          }

          shardProgress = '<span>' + arangoHelper.escapeHtml(shardProgress) + '</span>';
        } else {
          shardProgress = '<i class="fa fa-check-circle">';
          inSync++;
        }
        $('#' + collection + '-' + shard + ' .shardProgress').html(followersSyncing + shardProgress + working);
        total++;
      });

      if (total === inSync) {
        $('#' + collection + ' .shardSyncIcons i').addClass('fa-check-circle').removeClass('.fa-times-circle');
        $('#' + collection + ' .notInSync').addClass('inSync').removeClass('notInSync');
      } else {
        $('#' + collection + ' .shardSyncIcons i').addClass('fa-times-circle').removeClass('fa-check-circle');
      }
    },

    checkActiveShardDisplay: function () {
      var self = this;

      _.each($('.sectionShard'), function (elem) {
        if ($(elem).find('.sectionShardContent').is(':visible')) {
          self.getShardDetails($(elem).attr('id'));
        }
      });
    },

    getShardDetails: function (collection) {
      var self = this;

      var body = {
        collection: collection
      };

      $('#' + collection + ' .shardProgress').html(
        '<i class="fa fa-circle-o-notch fa-spin fa-fw"></i>'
      );

      $.ajax({
        type: 'PUT',
        cache: false,
        data: JSON.stringify(body),
        url: arangoHelper.databaseUrl('/_admin/cluster/collectionShardDistribution'),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: function (data) {
          self.renderShardDetail(collection, data);
        },
        error: function (data) {
        }
      });
    },

    render: function (navi) {
      if (window.location.hash === '#shards' && this.pending === false) {
        var self = this;
        self.pending = true;

        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_admin/cluster/shardDistribution'),
          contentType: 'application/json',
          processData: false,
          async: true,
          success: function (data) {
            self.pending = false;
            var collsAvailable = false;
            self.shardDistribution = data.results;

            _.each(data.results, function (ignore, name) {
              if (name !== 'error' && name !== 'code') {
                collsAvailable = true;
              }
            });

            if (collsAvailable) {
              self.continueRender(data.results);
            } else {
              arangoHelper.renderEmpty('No collections and no shards available');
            }
            self.checkActiveShardDisplay();
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
        fromServer = arangoHelper.getDatabaseServerId(fromServer);
      } else {
        leader = $(e.currentTarget).parent().parent().attr('leader');
        leader = arangoHelper.getDatabaseServerId(leader);
        fromServer = arangoHelper.getDatabaseServerId(from);
      }

      var buttons = [];
      var tableContent = [];

      var obj = {};
      var array = [];

      self.dbServers[0].fetch({
        success: function () {
          self.dbServers[0].each(function (db) {
            if (db.get('id') !== fromServer) {
              obj[db.get('name')] = {
                value: db.get('id'),
                label: db.get('name')
              };
            }
          });

          _.each(self.shardDistribution[collectionName].Plan[shardName].followers, function (follower) {
            delete obj[follower];
          });

          if (from) {
            delete obj[arangoHelper.getDatabaseShortName(leader)];
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
              'Please select the target database server. The selected database ' +
                'server will be the new leader of the shard.',
              array
            )
          );

          buttons.push(
            window.modalView.createSuccessButton(
              'Move',
              self.confirmMoveShards.bind(this, dbName, collectionName, shardName, fromServer)
            )
          );

          window.modalView.show(
            'modalTable.ejs',
            'Move shard: ' + shardName,
            buttons,
            tableContent
          );
        }
      });
    },

    confirmMoveShards: function (dbName, collectionName, shardName, fromServer) {
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
          if (data.id) {
            arangoHelper.arangoNotification('Shard ' + shardName + ' will be moved to ' + arangoHelper.getDatabaseShortName(toServer) + '.');
            window.setTimeout(function () {
              window.App.shardsView.render();
            }, 3000);
          }
        },
        error: function () {
          arangoHelper.arangoError('Shard ' + shardName + ' could not be moved to ' + arangoHelper.getDatabaseShortName(toServer) + '.');
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
            }, 3000);
            arangoHelper.arangoNotification('Started rebalance process.');
          }
        },
        error: function () {
          arangoHelper.arangoError('Could not start rebalance process.');
        }
      });

      window.modalView.hide();
    },

    continueRender: function (collections) {
      delete collections.code;
      delete collections.error;

      _.each(collections, function (attr, name) {
        // smart found
        var combined = {
          Plan: {},
          Current: {}
        };

        if (name.startsWith('_local_')) {
          // if prefix avail., get the collection name
          var cName = name.substr(7, name.length - 1);

          var toFetch = [
            '_local_' + cName,
            '_from_' + cName,
            '_to_' + cName,
            cName
          ];

          var pos = 0;
          _.each(toFetch, function (val, key) {
            if (collections[toFetch[pos]]) {
              _.each(collections[toFetch[pos]].Current, function (shardVal, shardName) {
                combined.Current[shardName] = shardVal;
              });
              _.each(collections[toFetch[pos]].Plan, function (shardVal, shardName) {
                combined.Plan[shardName] = shardVal;
              });
            }

            delete collections[toFetch[pos]];
            collections[cName] = combined;
            pos++;
          });
        }
      });

      // order results
      var ordered = {};
      Object.keys(collections).sort(function(l, r) {
        if (l[0] === '_' && r[0] !== '_') {
          return 1;
        } else if (l[0] !== '_' && r[0] === '_') {
          return -1;
        }
        return l === r ? 0 : ((l < r) ? -1 : 1);
      }).forEach(function (key) {
        ordered[key] = collections[key];
      });

      this.$el.html(this.template.render({
        collections: ordered,
        visible: this.visibleCollection
      }));

      // if we have only one collection to show, automatically open the entry
      if ($('.sectionShard').length === 1) {
        $('.sectionHeader').first().click();
      }

      // change the min height of innerContent - only for this view (temporary)
      $('.innerContent').css('min-height', '0px');
    },

    updateServerTime: function () {
      this.serverTime = new Date().getTime();
    }

  });
}());

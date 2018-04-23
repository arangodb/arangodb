/* jshint browser: true */
/* jshint unused: false */
/* global _, Backbone, btoa, templateEngine, $, window, arangoHelper */
(function () {
  'use strict';

  window.ReplicationView = Backbone.View.extend({
    el: '#content',

    // Modes:
    //  0: No active replication found.
    //  1: Replication per Database found.
    //  2: Replication per Server found.
    //  3: Active-Failover replication found.
    mode: null,

    // general info object, valid in all modes
    info: {
      state: null,
      mode: null,
      level: null,
      role: null,
      msg: 'There are no known issues'
    },

    // nodes info object, valid in active failover mode (3)
    nodes: {
      leader: null,
      followers: []
    },

    template: templateEngine.createTemplate('replicationView.ejs'),

    events: {
      'click #nodes-followers-id span': 'goToApplier'
    },

    render: function () {
      if (this.mode) {
        // mode found
        this.$el.html(this.template.render({
          mode: this.mode,
          info: this.info,
          nodes: this.nodes
        }));

        // fetching mode 3 related information
        if (this.mode === 3) {
          this.getEndpoints();
          this.getLoggerState();
        }
      } else {
        this.getMode(this.render.bind(this));
      }
    },

    goToApplier: function (e) {
      var endpoint = btoa($(e.currentTarget).attr('data'));
      window.App.navigate('#replication/applier/' + endpoint, {trigger: true});
    },

    getEndpoints: function () {
      var self = this;
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/cluster/endpoints'),
        contentType: 'application/json',
        success: function (data) {
          if (data.endpoints) {
            self.renderEndpoints(data.endpoints);
          } else {
            self.renderEndpoints();
          }
        },
        error: function () {
          self.renderEndpoints();
        }
      });
    },

    renderEndpoints: function (endpoints) {
      var self = this;

      if (endpoints) {
        var leader = endpoints[0];
        var followers = endpoints.slice(1, endpoints.length);

        $('#nodes-leader-id').html(leader.endpoint);
        $('#nodes-followers-id').html('');
        _.each(followers, function (follower) {
          $('#nodes-followers-id').append('<span data="' + self.parseEndpoint(follower.endpoint, true) + '">' + follower.endpoint + '</span>');
        });
      } else {
        $('#nodes-leader-id').html('Error');
        $('#nodes-followers-id').html('Error');
      }
    },

    parseEndpoint: function (endpoint, url) {
      var parsedEndpoint;
      if (endpoint.slice(6, 11) === '[::1]') {
        parsedEndpoint = window.location.host.split(':')[0] + ':' + endpoint.split(':')[4];
      }

      if (url) {
        parsedEndpoint = window.location.protocol + '//' + parsedEndpoint;
      }

      if (!parsedEndpoint) {
        return endpoint;
      }

      return parsedEndpoint;
    },

    getLoggerState: function () {
      var self = this;
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/replication/logger-state'),
        contentType: 'application/json',
        success: function (data) {
          self.renderLoggerState(data.server, data.clients, data.state);
        },
        error: function () {
          arangoHelper.arangoError('Replication', 'Could not fetch the masters logger state.');
        }
      });
    },

    renderLoggerState: function (server, clients, state) {
      if (server && clients && state) {
        $('#logger-running-id').html(state.running);
        $('#logger-version-id').html(server.version);
        $('#logger-serverid-id').html(server.serverId);
        $('#logger-time-id').html(state.time);
        $('#logger-lastLogTick-id').html(state.lastLogTick);
        $('#logger-lastUncommitedLogTick-id').html(state.lastUncommittedLogTick);
        $('#logger-totalEvents-id').html(state.totalEvents);
      } else {
        $('#logger-running-id').html('Error');
        $('#logger-endpoint-id').html('Error');
        $('#logger-version-id').html('Error');
        $('#logger-serverid-id').html('Error');
        $('#logger-time-id').html('Error');
      }
    },

    getMode: function (callback) {
      var self = this;
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/aardvark/replication/mode'),
        contentType: 'application/json',
        success: function (data) {
          if (data.mode) {
            if (Number.isInteger(data.mode)) {
              self.mode = data.mode;
              self.info.state = 'Replication is enabled';
            } else {
              self.mode = 'undefined';
            }
            if (self.mode === 3) {
              self.info.mode = 'Active-Failover';
              self.info.level = 'Server-wide replication';
              self.info.role = 'Leader';
            }
          }
          if (callback) {
            callback();
          }
        },
        error: function () {
          arangoHelper.arangoError('Replication', 'Could not fetch the replication state.');
        }
      });
    }

  });
}());

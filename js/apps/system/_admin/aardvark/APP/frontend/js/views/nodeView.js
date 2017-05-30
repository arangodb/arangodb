/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, templateEngine, arangoHelper, $, window */
(function () {
  'use strict';

  window.NodeView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('nodeView.ejs'),
    interval: 5000,
    dashboards: [],

    events: {
    },

    initialize: function (options) {
      if (window.App.isCluster) {
        this.coordinators = options.coordinators;
        this.dbServers = options.dbServers;
        this.coordid = options.coordid;
        this.updateServerTime();

        // start polling with interval
        /*
        window.setInterval(function () {
          if (window.location.hash.indexOf('#node/') === 0) {
          }
        }, this.interval);
       */
      }
    },

    breadcrumb: function (name) {
      $('#subNavigationBar .breadcrumb').html('Node: ' + name);
    },

    render: function () {
      this.$el.html(this.template.render({coords: []}));

      var callback = function () {
        this.continueRender();
        this.breadcrumb(arangoHelper.getCoordinatorShortName(this.coordid));
        // window.arangoHelper.buildNodeSubNav(this.coordname, 'Dashboard', 'Logs')
        $(window).trigger('resize');
      }.bind(this);

      if (!this.initCoordDone) {
        this.waitForCoordinators();
      }

      if (!this.initDBDone) {
        this.waitForDBServers(callback);
      } else {
        this.coordid = window.location.hash.split('/')[1];
        this.coordinator = this.coordinators.findWhere({id: this.coordid});
        callback();
      }
    },

    continueRender: function () {
      var self = this;

      if (!this.coordinator) {
        this.coordinator = this.coordinators.findWhere({id: this.coordid});
      }
      this.dashboards[this.coordinator.get('name')] = new window.DashboardView({
        dygraphConfig: window.dygraphConfig,
        database: window.App.arangoDatabase,
        serverToShow: {
          raw: this.coordinator.get('address'),
          isDBServer: false,
          endpoint: this.coordinator.get('protocol') + '://' + this.coordinator.get('address'),
          target: this.coordinator.get('id')
        }
      });
      this.dashboards[this.coordinator.get('name')].render();
      window.setTimeout(function () {
        self.dashboards[self.coordinator.get('name')].resize();
      }, 500);
    },

    waitForCoordinators: function (callback) {
      var self = this;

      window.setTimeout(function () {
        if (self.coordinators.length === 0) {
          self.waitForCoordinators(callback);
        } else {
          self.coordinator = self.coordinators.findWhere({name: self.coordid});
          self.initCoordDone = true;
          if (callback) {
            callback();
          }
        }
      }, 200);
    },

    waitForDBServers: function (callback) {
      var self = this;

      window.setTimeout(function () {
        if (self.dbServers[0].length === 0) {
          self.waitForDBServers(callback);
        } else {
          self.initDBDone = true;
          self.dbServer = self.dbServers[0];

          self.dbServer.each(function (model) {
            if (model.get('name') === 'DBServer001') {
              self.dbServer = model;
            }
          });

          callback();
        }
      }, 200);
    },

    updateServerTime: function () {
      this.serverTime = new Date().getTime();
    }

  });
}());

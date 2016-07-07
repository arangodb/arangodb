/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window, _ */
(function () {
  'use strict';

  window.NodesView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('nodesView.ejs'),
    interval: 5000,
    knownServers: [],

    events: {
      'click #nodesContent .pure-table-body .pure-table-row': 'navigateToNode'
    },

    initialize: function (options) {
      var self = this;
      clearInterval(this.intervalFunction);

      if (window.App.isCluster) {
        this.dbServers = options.dbServers;
        this.coordinators = options.coordinators;
        this.updateServerTime();
        this.toRender = options.toRender;

        // start polling with interval
        this.intervalFunction = window.setInterval(function () {
          if (window.location.hash === '#cNodes' || window.location.hash === '#dNodes' || window.location.hash === '#nodes') {
            self.checkNodesState();
          }
        }, this.interval);
      }
    },

    checkNodesState: function () {
      var callbackFunction = function (nodes) {
        _.each(nodes, function (node, name) {
          _.each($('.pure-table-row'), function (element) {
            if ($(element).attr('node') === name) {
              if (node.Status === 'GOOD') {
                $(element).removeClass('noHover');
                $(element).find('.state').html('<i class="fa fa-check-circle"></i>');
              } else {
                $(element).addClass('noHover');
                $(element).find('.state').html('<i class="fa fa-exclamation-circle"></i>');
              }
            }
          });
        });
      };

      // check cluster state
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/health'),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: function (data) {
          callbackFunction(data.Health);
        }
      });
    },

    navigateToNode: function (elem) {
      if (window.location.hash === '#dNodes') {
        return;
      }

      if ($(elem.currentTarget).hasClass('noHover')) {
        return;
      }

      var name = $(elem.currentTarget).attr('node');
      window.App.navigate('#node/' + encodeURIComponent(name), {trigger: true});
    },

    render: function () {
      var callback = function () {
        this.continueRender();
      }.bind(this);

      if (!this.initDoneCoords) {
        this.waitForCoordinators(callback);
      } else {
        callback();
      }
    },

    continueRender: function () {
      var coords;

      if (this.toRender === 'coordinator') {
        coords = this.coordinators.toJSON();
      } else {
        coords = this.dbServers.toJSON();
      }

      this.$el.html(this.template.render({
        coords: coords,
        type: this.toRender
      }));

      window.arangoHelper.buildNodesSubNav(this.toRender);
      this.checkNodesState();
    },

    waitForCoordinators: function (callback) {
      var self = this;

      window.setTimeout(function () {
        if (self.coordinators.length === 0) {
          self.waitForCoordinators(callback);
        } else {
          this.initDoneCoords = true;
          callback();
        }
      }, 200);
    },

    updateServerTime: function () {
      this.serverTime = new Date().getTime();
    }

  });
}());

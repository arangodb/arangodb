/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window, frontendConfig */
(function () {
  'use strict';

  window.ScaleView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('scaleView.ejs'),
    interval: 10000,
    knownServers: [],

    events: {
      'click #addCoord': 'addCoord',
      'click #removeCoord': 'removeCoord',
      'click #addDBs': 'addDBs',
      'click #removeDBs': 'removeDBs'
    },

    setCoordSize: function (number) {
      var self = this;
      var data = {
        numberOfCoordinators: number
      };

      $.ajax({
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_admin/cluster/numberOfServers'),
        contentType: 'application/json',
        data: JSON.stringify(data),
        success: function () {
          self.updateTable(data);
        },
        error: function () {
          arangoHelper.arangoError('Scale', 'Could not set coordinator size.');
        }
      });
    },

    setDBsSize: function (number) {
      var self = this;
      var data = {
        numberOfDBServers: number
      };

      $.ajax({
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_admin/cluster/numberOfServers'),
        contentType: 'application/json',
        data: JSON.stringify(data),
        success: function () {
          self.updateTable(data);
        },
        error: function () {
          arangoHelper.arangoError('Scale', 'Could not set coordinator size.');
        }
      });
    },

    addCoord: function () {
      this.setCoordSize(this.readNumberFromID('#plannedCoords', true));
    },

    removeCoord: function () {
      this.setCoordSize(this.readNumberFromID('#plannedCoords', false, true));
    },

    addDBs: function () {
      this.setDBsSize(this.readNumberFromID('#plannedDBs', true));
    },

    removeDBs: function () {
      this.setDBsSize(this.readNumberFromID('#plannedDBs', false, true));
    },

    readNumberFromID: function (id, increment, decrement) {
      var value = $(id).html();
      var parsed = false;

      try {
        parsed = JSON.parse(value);
      } catch (ignore) {}

      if (increment) {
        parsed++;
      }
      if (decrement) {
        if (parsed !== 1) {
          parsed--;
        }
      }

      return parsed;
    },

    initialize: function (options) {
      var self = this;
      clearInterval(this.intervalFunction);

      if (window.App.isCluster && frontendConfig.db === '_system') {
        this.dbServers = options.dbServers;
        this.coordinators = options.coordinators;
        this.updateServerTime();

        // start polling with interval
        this.intervalFunction = window.setInterval(function () {
          if (window.location.hash === '#sNodes') {
            self.coordinators.fetch({
              success: function () {
                self.dbServers.fetch({
                  success: function () {
                    self.continueRender(true);
                  }
                });
              }
            });
          }
        }, this.interval);
      }
    },

    render: function () {
      var self = this;

      var callback = function () {
        var cb2 = function () {
          self.continueRender();
        };

        this.waitForDBServers(cb2);
      }.bind(this);

      if (!this.initDoneCoords) {
        this.waitForCoordinators(callback);
      } else {
        callback();
      }

      window.arangoHelper.buildNodesSubNav('scale');
    },

    continueRender: function (rerender) {
      var coords;
      var dbs;
      var self = this;
      coords = this.coordinators.toJSON();
      dbs = this.dbServers.toJSON();

      this.$el.html(this.template.render({
        runningCoords: coords.length,
        runningDBs: dbs.length,
        plannedCoords: undefined,
        plannedDBs: undefined,
        initialized: rerender
      }));

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/numberOfServers'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          self.updateTable(data);
        }
      });
    },

    updateTable: function (data) {
      var scalingActive = '<span class="warning">scaling in progress <i class="fa fa-circle-o-notch fa-spin"></i></span>';
      var scalingDone = '<span class="positive">no scaling process active</span>';

      if (data.numberOfCoordinators) {
        $('#plannedCoords').html(data.numberOfCoordinators);

        if (this.coordinators.toJSON().length === data.numberOfCoordinators) {
          $('#statusCoords').html(scalingDone);
        } else {
          $('#statusCoords').html(scalingActive);
        }
      }

      if (data.numberOfDBServers) {
        $('#plannedDBs').html(data.numberOfDBServers);
        if (this.dbServers.toJSON().length === data.numberOfDBServers) {
          $('#statusDBs').html(scalingDone);
        } else {
          $('#statusDBs').html(scalingActive);
        }
      }
    },

    waitForDBServers: function (callback) {
      var self = this;

      if (this.dbServers.length === 0) {
        window.setInterval(function () {
          self.waitForDBServers(callback);
        }, 300);
      } else {
        callback();
      }
    },

    waitForCoordinators: function (callback) {
      var self = this;

      window.setTimeout(function () {
        if (self.coordinators.length === 0) {
          self.waitForCoordinators(callback);
        } else {
          self.initDoneCoords = true;
          callback();
        }
      }, 200);
    },

    updateServerTime: function () {
      this.serverTime = new Date().getTime();
    }

  });
}());

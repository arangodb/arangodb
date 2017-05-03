/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, $, Backbone, templateEngine, window */
(function () {
  'use strict';

  window.NodeInfoView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('nodeInfoView.ejs'),

    initialize: function (options) {
      if (window.App.isCluster) {
        this.nodeId = options.nodeId;
        this.dbServers = options.dbServers;
        this.coordinators = options.coordinators;
      }
    },

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    render: function () {
      this.$el.html(this.template.render({entries: []}));

      var callback = function () {
        this.continueRender();
        this.breadcrumb(arangoHelper.getCoordinatorShortName(this.nodeId));
        $(window).trigger('resize');
      }.bind(this);

      if (!this.initCoordDone) {
        this.waitForCoordinators();
      }

      if (!this.initDBDone) {
        this.waitForDBServers(callback);
      } else {
        this.nodeId = window.location.hash.split('/')[1];
        this.coordinator = this.coordinators.findWhere({name: this.coordname});
        callback();
      }
    },

    continueRender: function () {
      var model;
      if (this.coordinator) {
        model = this.coordinator.toJSON();
      } else {
        model = this.dbServer.toJSON();
      }

      var renderObj = {};
      if (model.name) {
        renderObj.Name = model.name;
      }
      if (model.address) {
        renderObj.Address = model.address;
      }
      if (model.status) {
        renderObj.Status = model.status;
      }
      if (model.protocol) {
        renderObj.Protocol = model.protocol;
      }
      if (model.role) {
        renderObj.Role = model.role;
      }
      this.$el.html(this.template.render({entries: renderObj}));
    },

    breadcrumb: function (name) {
      $('#subNavigationBar .breadcrumb').html('Node: ' + name);
    },

    waitForCoordinators: function (callback) {
      var self = this;

      window.setTimeout(function () {
        if (self.coordinators.length === 0) {
          self.waitForCoordinators(callback);
        } else {
          self.coordinator = self.coordinators.findWhere({name: self.nodeId});
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
        if (self.dbServers.length === 0) {
          self.waitForDBServers(callback);
        } else {
          self.initDBDone = true;

          self.dbServers.each(function (model) {
            if (model.get('id') === self.nodeId) {
              self.dbServer = model;
            }
          });

          callback();
        }
      }, 200);
    }

  });
}());

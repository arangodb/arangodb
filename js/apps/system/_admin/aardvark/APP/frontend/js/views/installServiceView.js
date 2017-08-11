/* jshint browser: true */
/* global Backbone, $, window, arangoHelper, templateEngine, _ */
(function () {
  'use strict';

  window.ServiceInstallView = Backbone.View.extend({
    el: '#content',
    readOnly: false,

    template: templateEngine.createTemplate('serviceInstallView.ejs'),

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
    },

    initialize: function () {
    },

    waitForResponse: function () {
      var self = this;
      window.setTimeout(function () {
        self.render();
      }, 200);
    },

    createSubViews: function () {
      var self = this;
      this._installedSubViews = { };

      self.collection.each(function (foxx) {
        var subView = new window.FoxxRepoView({
          model: foxx,
          appsView: self
        });
        self._installedSubViews[foxx.get('name')] = subView;
      });
    },

    render: function () {
      // if repo not fetched yet, wait
      $(this.el).html(this.template.render({
        services: this.collection
      }));

      arangoHelper.buildServicesSubNav('Store');

      if (this.collection.length === 0) {
        this.waitForResponse();
        return;
      }

      this.collection.sort();
      this.createSubViews();

      _.each(this._installedSubViews, function (v) {
        $('#availableFoxxes').append(v.render());
      });

      // this.delegateEvents();
      // arangoHelper.checkDatabasePermissions(this.setReadOnly.bind(this));
      return this;
    }
  });
}());

/* jshint browser: true */
/* global frontendConfig, Backbone, $, window, arangoHelper, templateEngine, _ */
(function () {
  'use strict';

  window.ServiceInstallView = Backbone.View.extend({
    el: '#content',
    readOnly: false,
    foxxStoreRetry: 0,

    template: templateEngine.createTemplate('serviceInstallView.ejs'),

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
      'click #categorySelection': 'renderCategories',
      'click #foxxFilters': 'resetFilters',
      'keyup #foxxSearch': 'search'
    },

    initialize: function (options) {
      this.functionsCollection = options.functionsCollection;

      if (window.App.replaceApp) {
        this._upgrade = true;
      }
    },

    fetchStore: function () {
      var self = this;
      this.foxxStoreRetry = 1;
      this.collection.fetch({
        cache: false,
        success: function () {
          if (window.location.hash === '#services/install') {
            self.render();
          }
        }
      });
    },

    search: function () {
      this._installedSubViews[Object.keys(this._installedSubViews)[0]].applyFilter();

      var text = $('#foxxSearch').val();
      if (text) {
        _.each(this._installedSubViews, function (view) {
          if (view.model.get('name').includes(text)) {
            if ($(view.el).attr('shown') === 'true') {
              $(view.el).show();
            }
          } else {
            $(view.el).hide();
          }
        });
      } else {
        this._installedSubViews[Object.keys(this._installedSubViews)[0]].applyFilter();
      }
    },

    createSubViews: function () {
      var self = this;
      this._installedSubViews = { };

      self.collection.each(function (foxx) {
        var subView = new window.FoxxRepoView({
          collection: self.functionsCollection,
          model: foxx,
          appsView: self,
          upgrade: self._upgrade
        });
        self._installedSubViews[foxx.get('name')] = subView;
      });
    },

    renderCategories: function (e) {
      this._installedSubViews[Object.keys(this._installedSubViews)[0]].renderCategories(e);
    },

    resetFilters: function () {
      $('#foxxSearch').val('');
      this._installedSubViews[Object.keys(this._installedSubViews)[0]].resetFilters();
    },

    render: function () {
      // if repo not fetched yet, wait
      $(this.el).html(this.template.render({
        services: this.collection
      }));

      arangoHelper.buildServicesSubNav('Store');
      this.breadcrumb();

      if (this.collection.length === 0 && this.foxxStoreRetry === 0) {
        this.fetchStore();
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
    },

    breadcrumb: function () {
      var replaceString = 'New';
      if (this._upgrade) {
        replaceString = 'Replace (' + window.App.replaceAppData.mount + ')';
      }
      $('#subNavigationBar .breadcrumb').html(
        '<a href="#services">Services:</a> ' + replaceString
      );
    }
  });
}());

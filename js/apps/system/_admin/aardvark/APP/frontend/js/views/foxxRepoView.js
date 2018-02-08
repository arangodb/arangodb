/* jshint browser: true */
/* jshint unused: false */
/* global _, Backbone, $, window, templateEngine, arangoHelper */

(function () {
  'use strict';

  window.FoxxRepoView = Backbone.View.extend({
    tagName: 'div',
    className: 'foxxTile tile pure-u-1-1 pure-u-sm-1-2 pure-u-md-1-3 pure-u-lg-1-4 pure-u-xl-1-5',
    template: templateEngine.createTemplate('foxxRepoView.ejs'),
    _show: true,

    categories: [],

    events: {
      'click .installFoxx': 'installStoreService',
      'click .foxxTile': 'openAppDetailView'
    },

    initialize: function () {
      this.categories = [];
      this.fetchCategories();
    },

    openAppDetailView: function () {
      window.App.navigate('service/detail/' + encodeURIComponent(this.model.get('name')), { trigger: true });
    },

    render: function () {
      this.model.fetchThumbnail(function () {
        $(this.el).html(this.template.render({
          model: this.model.toJSON()
        }));
      }.bind(this));
      $(this.el).attr('category', 'general');

      return $(this.el);
    },

    installStoreService: function (e) {
      this.toInstall = $(e.currentTarget).attr('appId');
      this.version = $(e.currentTarget).attr('appVersion');
      arangoHelper.createMountPointModal(this.installFoxxFromStore.bind(this));
    },

    categoriesCallbackFunction: function (options) {
      this.categoryOptions = options;
      this.applyFilter();
    },

    fetchCategories: function () {
      var self = this;

      _.each(['general', 'security', 'helper', 'tutorial'], function (level) {
        self.categories.push(level);
      });
    },

    renderCategories: function (e) {
      var self = this;

      if (!this.categoryOptions) {
        this.categoryOptions = {};
      }

      var active;
      _.each(this.categories, function (topic, name) {
        if (self.categoryOptions[name]) {
          active = self.categoryOptions[name].active;
        }
        self.categoryOptions[topic] = {
          name: topic,
          active: active || false
        };
      });

      var pos = $(e.currentTarget).position();
      pos.right = '30';

      this.categoryView = new window.FilterSelectView({
        name: 'Category',
        options: self.categoryOptions,
        position: pos,
        callback: self.categoriesCallbackFunction.bind(this),
        multiple: false
      });
      this.categoryView.render();
    },

    resetFilters: function () {
      _.each(this.categoryOptions, function (option) {
        option.active = false;
      });
      this.applyFilter();
    },

    isFilterActive: function (filterobj) {
      var active = false;
      _.each(filterobj, function (obj) {
        if (obj.active) {
          active = true;
        }
      });
      return active;
    },

    applyFilter: function () {
      var self = this;
      var isCategory = this.isFilterActive(this.categoryOptions);

      if (isCategory) {
        // both filters active
        _.each($('#availableFoxxes').children(), function (entry) {
          if (self.categoryOptions[$(entry).attr('category')].active === false) {
            $(entry).hide();
            $(entry).attr('shown', 'false');
          } else {
            $(entry).show();
            $(entry).attr('shown', 'true');
          }
        });
        this.changeButton('category');
      } else {
        _.each($('#availableFoxxes').children(), function (entry) {
          $(entry).show();
          $(entry).attr('shown', 'true');
        });
        this.changeButton();
      }

      var count = 0;
      _.each($('#logEntries').children(), function (elem) {
        if ($(elem).css('display') === 'block') {
          count++;
        }
      });
      if (count === 1) {
        $('.logBorder').css('visibility', 'hidden');
        $('#noLogEntries').hide();
      } else if (count === 0) {
        $('#noLogEntries').show();
      } else {
        $('.logBorder').css('visibility', 'visible');
        $('#noLogEntries').hide();
      }
    },

    changeButton: function (btn) {
      if (!btn) {
        $('#categorySelection').addClass('button-default').removeClass('button-success');
        $('#foxxFilters').hide();
        $('#filterDesc').html('');
      } else {
        $('#categorySelection').addClass('button-success').removeClass('button-default');
        $('#filterDesc').html(btn);
        $('#foxxFilters').show();
      }
    },

    installFoxxFromStore: function (e) {
      if (window.modalView.modalTestAll()) {
        var mount, flag;
        if (this._upgrade) {
          mount = this.mount;
          flag = $('#new-app-teardown').prop('checked');
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
          if (mount.charAt(0) !== '/') {
            mount = '/' + mount;
          }
        }
        if (flag !== undefined) {
          this.collection.installFromStore({name: this.toInstall, version: this.version}, mount, this.installCallback.bind(this), flag);
        } else {
          this.collection.installFromStore({name: this.toInstall, version: this.version}, mount, this.installCallback.bind(this));
        }
        window.modalView.hide();
        arangoHelper.arangoNotification('Services', 'Installing ' + this.toInstall + '.');
        this.toInstall = null;
        this.version = null;
      }
    },

    installCallback: function (result) {
      window.App.navigate('#services', {trigger: true});
      window.App.applicationsView.installCallback(result);
    }

  });
}());

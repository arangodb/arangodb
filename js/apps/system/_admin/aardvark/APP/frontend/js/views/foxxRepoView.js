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
      'click .borderBox': 'goToServiceDetail',
      'click img': 'goToServiceDetail',
      'click .foxxDesc': 'goToServiceDetail'
    },

    initialize: function (options) {
      this.categories = [];
      this.fetchCategories();
      if (options.functionsCollection) {
        this.functionsCollection = options.functionsCollection;
      }
      if (options.upgrade) {
        this._upgrade = options.upgrade;
      }
    },

    openAppDetailView: function () {
      window.App.navigate('service/detail/' + encodeURIComponent(this.model.get('name')), { trigger: true });
    },

    render: function () {
      this.model.fetchThumbnail(function (error) {
        if (error) {
          $(this.el).remove();
        }

        var thumbnailUrl = this.model.get('defaultThumbnailUrl');
        if (this.model.get('manifest')) {
          try {
            if (this.model.get('manifest').thumbnail) {
              thumbnailUrl = this.model.getThumbnailUrl();
            }
          } catch (ignore) {
          }
        }
        $(this.el).html(this.template.render({
          model: this.model.toJSON(),
          thumbnail: thumbnailUrl,
          upgrade: this._upgrade
        }));
      }.bind(this));

      if (this.model.get('categories')) {
        // set categories for each foxx
        $(this.el).attr('category', this.model.get('categories'));
      }

      return $(this.el);
    },

    goToServiceDetail: function (e) {
      window.App.navigate('store/' + encodeURIComponent(this.model.get('name')), {trigger: true});
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

    onlyUnique: function (value, index, self) {
      return self.indexOf(value) === index;
    },

    fetchCategories: function () {
      var self = this;

      var tmpArray = [];
      _.each(window.App.foxxRepo.models, function (foxx) {
        _.each(foxx.get('categories'), function (category) {
          tmpArray.push(category);
        });
      });
      // remove duplicates
      self.categories = tmpArray.filter(this.onlyUnique);
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
          var categories = $(entry).attr('category').split(',');
          var found = false;
          _.each(categories, function (category) {
            if (self.categoryOptions[category].active === true) {
              found = true;
            }
          });

          if (!found) {
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
        var mount, info, options;
        if (this._upgrade) {
          mount = window.App.replaceAppData.mount;
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
          if (mount.charAt(0) !== '/') {
            mount = '/' + mount;
          }
        }

        info = {
          name: this.toInstall,
          version: this.version
        };

        options = arangoHelper.getFoxxFlags();
        this.collection.install('store', info, mount, options, this.installCallback.bind(this));
        window.modalView.hide();

        if (this._upgrade) {
          arangoHelper.arangoNotification('Services', 'Upgrading ' + this.toInstall + '.');
        } else {
          arangoHelper.arangoNotification('Services', 'Installing ' + this.toInstall + '.');
        }
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

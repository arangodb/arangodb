/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, window, templateEngine, arangoHelper */

(function () {
  'use strict';

  window.FoxxRepoView = Backbone.View.extend({
    tagName: 'div',
    className: 'foxxTile tile pure-u-1-1 pure-u-sm-1-2 pure-u-md-1-3 pure-u-lg-1-4 pure-u-xl-1-5',
    template: templateEngine.createTemplate('foxxRepoView.ejs'),
    _show: true,

    events: {
      'click .installFoxx': 'installStoreService',
      'click .foxxTile': 'openAppDetailView'
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

      return $(this.el);
    },

    installStoreService: function (e) {
      this.toInstall = $(e.currentTarget).attr('appId');
      this.version = $(e.currentTarget).attr('appVersion');
      arangoHelper.createMountPointModal(this.installFoxxFromStore.bind(this));
    },

    installFoxxFromStore: function (e) {
      if (window.modalView.modalTestAll()) {
        var mount, flag;
        if (this._upgrade) {
          mount = this.mount;
          flag = $('#new-app-teardown').prop('checked');
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
        }
        if (flag !== undefined) {
          this.collection.installFromStore({name: this.toInstall, version: this.version}, mount, this.installCallback.bind(this), flag);
        } else {
          this.collection.installFromStore({name: this.toInstall, version: this.version}, mount, this.installCallback.bind(this));
        }
        window.modalView.hide();
        this.toInstall = null;
        this.version = null;
        arangoHelper.arangoNotification('Services', 'Installing ' + this.toInstall + '.');
      }
    },

    installCallback: function (result) {
      window.App.navigate('#services', {trigger: true});
      window.App.applicationsView.installCallback(result);
    }

  });
}());

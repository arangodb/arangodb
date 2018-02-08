/* jshint browser: true */
/* global Backbone, $, window, arangoHelper, templateEngine */
(function () {
  'use strict';

  window.ServiceInstallUploadView = Backbone.View.extend({
    el: '#content',
    readOnly: false,

    template: templateEngine.createTemplate('serviceInstallUploadView.ejs'),

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
      'click #installUploadService': 'installFromUpload'
    },

    render: function () {
      $(this.el).html(this.template.render({
        services: this.collection
      }));

      this.prepareUpload();
      this.breadcrumb();
      arangoHelper.createTooltips('.modalTooltips');

      return this;
    },

    installFromUpload: function () {
      arangoHelper.createMountPointModal(this.installFoxxFromZip.bind(this));
    },

    testFunction: function (files, data) {
      if (!window.foxxData) {
        window.foxxData = {};
      }
      window.foxxData.files = files;
      window.foxxData.data = data;
      $('#installUploadService').attr('disabled', false);
    },

    prepareUpload: function () {
      var self = this;

      $('#upload-foxx-zip').uploadFile({
        url: arangoHelper.databaseUrl('/_api/upload?multipart=true'),
        allowedTypes: 'zip, js',
        multiple: false,
        onSuccess: self.testFunction
      });
    },

    installFoxxFromZip: function () {
      if (window.foxxData.data === undefined) {
        window.foxxData.data = this._uploadData;
      } else {
        this._uploadData = window.foxxData.data;
      }
      if (window.foxxData.data && window.modalView.modalTestAll()) {
        var mount, flag, isLegacy;
        if (this._upgrade) {
          mount = this.mount;
          flag = Boolean($('#new-app-teardown').prop('checked'));
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
          if (mount.charAt(0) !== '/') {
            mount = '/' + mount;
          }
        }
        isLegacy = Boolean($('#zip-app-islegacy').prop('checked'));
        this.collection.installFromZip(window.foxxData.data.filename, mount, this.installCallback.bind(this), isLegacy, flag);
      }
      window.modalView.hide();
    },

    installCallback: function (result) {
      window.App.navigate('#services', {trigger: true});
      window.App.applicationsView.installCallback(result);
    },

    breadcrumb: function () {
      var self = this;

      if (window.App.naviView) {
        $('#subNavigationBar .breadcrumb').html(
          '<a href="#services">Services:</a> New'
        );
        arangoHelper.buildServicesSubNav('Upload');
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    }

  });
}());

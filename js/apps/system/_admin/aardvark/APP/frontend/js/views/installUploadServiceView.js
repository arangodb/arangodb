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

    initialize: function () {
      if (window.App.replaceApp) {
        this._upgrade = true;
      }
    },

    events: {
      'click #installUploadService': 'installFromUpload'
    },

    render: function () {
      $(this.el).html(this.template.render({
        services: this.collection,
        upgrade: this._upgrade
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
        allowedTypes: 'zip,js',
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
          zipFile: window.foxxData.data.filename
        };

        options = arangoHelper.getFoxxFlags();
        options.legacy = Boolean($('#zip-app-islegacy')[0].checked);
        this.collection.install('zip', info, mount, options, this.installCallback.bind(this));
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
        var replaceString = 'New';
        if (this._upgrade) {
          replaceString = 'Replace (' + window.App.replaceAppData.mount + ')';
        }
        $('#subNavigationBar .breadcrumb').html(
          '<a href="#services">Services:</a> ' + replaceString
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

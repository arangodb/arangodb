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
    },

    initialize: function () {
    },

    render: function () {
      $(this.el).html(this.template.render({
        services: this.collection
      }));

      this.prepareUpload();
      this.breadcrumb();

      return this;
    },

    prepareUpload: function () {
      $('#upload-foxx-zip').uploadFile({
        url: arangoHelper.databaseUrl('/_api/upload?multipart=true'),
        allowedTypes: 'zip,js',
        multiple: false,
        onSuccess: this.installFoxxFromZip
      });
    },

    installFoxxFromZip: function (files, data) {
      if (data === undefined) {
        data = this._uploadData;
      } else {
        this._uploadData = data;
      }
      if (data && window.modalView.modalTestAll()) {
        var mount, flag, isLegacy;
        if (this._upgrade) {
          mount = this.mount;
          flag = Boolean($('#new-app-teardown').prop('checked'));
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
        }
        isLegacy = Boolean($('#zip-app-islegacy').prop('checked'));
        this.collection.installFromZip(data.filename, mount, this.installCallback.bind(this), isLegacy, flag);
      }
    },

    installCallback: function (result) {
      var self = this;
      var errors = {
        'ERROR_SERVICE_DOWNLOAD_FAILED': { 'code': 1752, 'message': 'service download failed' }
      };

      if (result.error === false) {
        this.collection.fetch({
          success: function () {
            window.modalView.hide();
            self.reload();
            arangoHelper.arangoNotification('Services', 'Service ' + result.name + ' installed.');
          }
        });
      } else {
        var res = result;
        if (result.hasOwnProperty('responseJSON')) {
          res = result.responseJSON;
        }
        switch (res.errorNum) {
          case errors.ERROR_SERVICE_DOWNLOAD_FAILED.code:
            arangoHelper.arangoError('Services', 'Unable to download application from the given repository.');
            break;
          default:
            arangoHelper.arangoError('Services', res.errorNum + '. ' + res.errorMessage);
        }
      }
    },

    breadcrumb: function () {
      var self = this;

      if (window.App.naviView) {
        $('#subNavigationBar .breadcrumb').html(
          'New Service'
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

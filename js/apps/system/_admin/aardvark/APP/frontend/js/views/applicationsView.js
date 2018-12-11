/* jshint browser: true */
/* global Backbone, $, window, arangoHelper, templateEngine, _ */
(function () {
  'use strict';

  window.ApplicationsView = Backbone.View.extend({
    el: '#content',
    readOnly: false,

    template: templateEngine.createTemplate('applicationsView.ejs'),

    events: {
      'click #addApp': 'createInstallModal',
      'click #foxxToggle': 'slideToggle',
      'click #checkDevel': 'toggleDevel',
      'click #checkProduction': 'toggleProduction',
      'click #checkSystem': 'toggleSystem'
    },

    fixCheckboxes: function () {
      if (this._showDevel) {
        $('#checkDevel').attr('checked', 'checked');
      } else {
        $('#checkDevel').removeAttr('checked');
      }
      if (this._showSystem) {
        $('#checkSystem').attr('checked', 'checked');
      } else {
        $('#checkSystem').removeAttr('checked');
      }
      if (this._showProd) {
        $('#checkProduction').attr('checked', 'checked');
      } else {
        $('#checkProduction').removeAttr('checked');
      }
      $('#checkDevel').next().removeClass('fa fa-check-square-o fa-square-o').addClass('fa');
      $('#checkSystem').next().removeClass('fa fa-check-square-o fa-square-o').addClass('fa');
      $('#checkProduction').next().removeClass('fa fa-check-square-o fa-square-o').addClass('fa');
      arangoHelper.setCheckboxStatus('#foxxDropdown');
    },

    toggleDevel: function () {
      var self = this;
      this._showDevel = !this._showDevel;
      _.each(this._installedSubViews, function (v) {
        v.toggle('devel', self._showDevel);
      });
      this.fixCheckboxes();
    },

    toggleProduction: function () {
      var self = this;
      this._showProd = !this._showProd;
      _.each(this._installedSubViews, function (v) {
        v.toggle('production', self._showProd);
      });
      this.fixCheckboxes();
    },

    toggleSystem: function () {
      this._showSystem = !this._showSystem;
      var self = this;
      _.each(this._installedSubViews, function (v) {
        v.toggle('system', self._showSystem);
      });
      this.fixCheckboxes();
    },

    reload: function () {
      var self = this;

      // unbind and remove any unused views
      _.each(this._installedSubViews, function (v) {
        v.undelegateEvents();
      });

      this.collection.fetch({
        success: function () {
          self.createSubViews();
          self.render();
        }
      });
    },

    createSubViews: function () {
      var self = this;
      this._installedSubViews = { };

      self.collection.each(function (foxx) {
        var subView = new window.FoxxActiveView({
          model: foxx,
          appsView: self
        });
        self._installedSubViews[foxx.get('mount')] = subView;
      });
    },

    initialize: function () {
      this._installedSubViews = {};
      this._showDevel = true;
      this._showProd = true;
      this._showSystem = false;
    },

    slideToggle: function () {
      $('#foxxToggle').toggleClass('activated');
      $('#foxxDropdownOut').slideToggle(200);
    },

    createInstallModal: function (event) {
      if (!this.readOnly) {
        event.preventDefault();
        window.App.navigate('services/install', {trigger: true});
      }
    },

    setReadOnly: function () {
      this.readOnly = true;
      $('#addApp').parent().parent().addClass('disabled');
      $('#addApp').addClass('disabled');
    },

    render: function () {
      this.collection.sort();

      $(this.el).html(this.template.render({}));
      _.each(this._installedSubViews, function (v) {
        $('#installedList').append(v.render());
      });
      this.delegateEvents();
      $('#checkDevel').attr('checked', this._showDevel);
      $('#checkProduction').attr('checked', this._showProd);
      $('#checkSystem').attr('checked', this._showSystem);
      arangoHelper.setCheckboxStatus('#foxxDropdown');

      var self = this;
      _.each(this._installedSubViews, function (v) {
        v.toggle('devel', self._showDevel);
        v.toggle('system', self._showSystem);
      });

      arangoHelper.fixTooltips('icon_arangodb', 'left');
      arangoHelper.checkDatabasePermissions(this.setReadOnly.bind(this));
      return this;
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
    }

  });
}());

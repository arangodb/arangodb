/* jshint browser: true */
/* global $, Joi, _, arangoHelper, templateEngine, window */
(function () {
  'use strict';

  // mop: copy paste from common/bootstrap/errors.js
  var errors = {
    'ERROR_SERVICE_DOWNLOAD_FAILED': { 'code': 1752, 'message': 'service download failed' }
  };

  var appStoreTemplate = templateEngine.createTemplate('applicationListView.ejs');

  var FoxxInstallView = function (opts) {
    this.collection = opts.collection;
  };

  var installCallback = function (result) {
    var self = this;

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
  };

  var setMountpointValidators = function () {
    window.modalView.modalBindValidation({
      id: 'new-app-mount',
      validateInput: function () {
        return [
          {
            rule: Joi.string().regex(/(\/|^)APP(\/|$)/i, {invert: true}),
            msg: 'May not contain /APP'
          },
          {
            rule: Joi.string().regex(/^(\/[a-zA-Z0-9_\-%]+)+$/),
            msg: 'Can only contain [a-zA-Z0-9_-%]'
          },
          {
            rule: Joi.string().regex(/^\/([^_]|_open\/)/),
            msg: 'Mountpoints with _ are reserved for internal use'
          },
          {
            rule: Joi.string().regex(/[^/]$/),
            msg: 'May not end with /'
          },
          {
            rule: Joi.string().regex(/^\//),
            msg: 'Has to start with /'
          },
          {
            rule: Joi.string().required().min(2),
            msg: 'Has to be non-empty'
          }
        ];
      }
    });
  };

  var setGithubValidators = function () {
    window.modalView.modalBindValidation({
      id: 'repository',
      validateInput: function () {
        return [
          {
            rule: Joi.string().required().regex(/^[a-zA-Z0-9_-]+\/[a-zA-Z0-9_-]+$/),
            msg: 'No valid Github account and repository.'
          }
        ];
      }
    });
  };

  var setNewAppValidators = function () {
    window.modalView.modalBindValidation({
      id: 'new-app-author',
      validateInput: function () {
        return [
          {
            rule: Joi.string().required().min(1),
            msg: 'Has to be non empty.'
          }
        ];
      }
    });
    window.modalView.modalBindValidation({
      id: 'new-app-name',
      validateInput: function () {
        return [
          {
            rule: Joi.string().required().regex(/^[a-zA-Z\-_][a-zA-Z0-9\-_]*$/),
            msg: "Can only contain a to z, A to Z, 0-9, '-' and '_'. Cannot start with a number."
          }
        ];
      }
    });

    window.modalView.modalBindValidation({
      id: 'new-app-description',
      validateInput: function () {
        return [
          {
            rule: Joi.string().required().min(1),
            msg: 'Has to be non empty.'
          }
        ];
      }
    });

    window.modalView.modalBindValidation({
      id: 'new-app-license',
      validateInput: function () {
        return [
          {
            rule: Joi.string().required().regex(/^[a-zA-Z0-9 .,;-]+$/),
            msg: "Can only contain a to z, A to Z, 0-9, '-', '.', ',' and ';'."
          }
        ];
      }
    });
    window.modalView.modalTestAll();
  };

  var switchTab = function (openTab) {
    window.modalView.clearValidators();
    var button = $('#modalButton1');
    if (!this._upgrade) {
      setMountpointValidators();
    }
    switch (openTab) {
      case 'newApp':
        button.html('Generate');
        button.prop('disabled', false);
        setNewAppValidators();
        break;
      case 'appstore':
        button.html('Install');
        button.prop('disabled', true);
        break;
      case 'github':
        setGithubValidators();
        button.html('Install');
        button.prop('disabled', false);
        break;
      case 'zip':
        button.html('Install');
        button.prop('disabled', false);
        break;
      default:
    }

    if (!button.prop('disabled') && !window.modalView.modalTestAll()) {
      // trigger the validation so the "ok" button has the correct state
      button.prop('disabled', true);
    }
  };

  var switchModalButton = function (event) {
    var openTab = $(event.currentTarget).attr('href').substr(1);
    switchTab.call(this, openTab);
  };

  var installFoxxFromStore = function (e) {
    switchTab.call(this, 'appstore');
    if (window.modalView.modalTestAll()) {
      var mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop('checked');
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
      var toInstall = $(e.currentTarget).attr('appId');
      var version = $(e.currentTarget).attr('appVersion');
      if (flag !== undefined) {
        this.collection.installFromStore({name: toInstall, version: version}, mount, installCallback.bind(this), flag);
      } else {
        this.collection.installFromStore({name: toInstall, version: version}, mount, installCallback.bind(this));
      }
      window.modalView.hide();
      arangoHelper.arangoNotification('Services', 'Installing ' + toInstall + '.');
    }
  };

  var installFoxxFromZip = function (files, data) {
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
      this.collection.installFromZip(data.filename, mount, installCallback.bind(this), isLegacy, flag);
    }
  };

  var installFoxxFromGithub = function () {
    if (window.modalView.modalTestAll()) {
      var url, version, mount, flag, isLegacy;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop('checked');
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
      url = window.arangoHelper.escapeHtml($('#repository').val());
      version = window.arangoHelper.escapeHtml($('#tag').val());

      if (version === '') {
        version = 'master';
      }
      var info = {
        url: window.arangoHelper.escapeHtml($('#repository').val()),
        version: window.arangoHelper.escapeHtml($('#tag').val())
      };

      try {
        Joi.assert(url, Joi.string().regex(/^[a-zA-Z0-9_-]+\/[a-zA-Z0-9_-]+$/));
      } catch (e) {
        return;
      }
      // send server req through collection
      isLegacy = Boolean($('#github-app-islegacy').prop('checked'));
      this.collection.installFromGithub(info, mount, installCallback.bind(this), isLegacy, flag);
    }
  };

  var generateNewFoxxApp = function () {
    if (window.modalView.modalTestAll()) {
      var mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop('checked');
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
      var info = {
        name: window.arangoHelper.escapeHtml($('#new-app-name').val()),
        documentCollections: _.map($('#new-app-document-collections').select2('data'), function (d) {
          return window.arangoHelper.escapeHtml(d.text);
        }),
        edgeCollections: _.map($('#new-app-edge-collections').select2('data'), function (d) {
          return window.arangoHelper.escapeHtml(d.text);
        }),
        //        authenticated: window.arangoHelper.escapeHtml($("#new-app-name").val()),
        author: window.arangoHelper.escapeHtml($('#new-app-author').val()),
        license: window.arangoHelper.escapeHtml($('#new-app-license').val()),
        description: window.arangoHelper.escapeHtml($('#new-app-description').val())
      };
      this.collection.generate(info, mount, installCallback.bind(this), flag);
    }
  };

  var addAppAction = function () {
    var openTab = $('.modal-body .tab-pane.active').attr('id');
    switch (openTab) {
      case 'newApp':
        generateNewFoxxApp.apply(this);
        break;
      case 'github':
        installFoxxFromGithub.apply(this);
        break;
      case 'zip':
        installFoxxFromZip.apply(this);
        break;
      default:
    }
  };

  var render = function (scope, upgrade) {
    var buttons = [];
    var modalEvents = {
      'click #infoTab a': switchModalButton.bind(scope),
      'click .install-app': installFoxxFromStore.bind(scope)
    };
    buttons.push(
      window.modalView.createSuccessButton('Generate', addAppAction.bind(scope))
    );
    window.modalView.show(
      'modalApplicationMount.ejs',
      'Install Service',
      buttons,
      upgrade,
      undefined,
      undefined,
      modalEvents
    );
    $('#new-app-document-collections').select2({
      tags: [],
      showSearchBox: false,
      minimumResultsForSearch: -1,
      width: '336px'
    });
    $('#new-app-edge-collections').select2({
      tags: [],
      showSearchBox: false,
      minimumResultsForSearch: -1,
      width: '336px'
    });

    var checkButton = function () {
      var button = $('#modalButton1');
      if (!button.prop('disabled') && !window.modalView.modalTestAll()) {
        button.prop('disabled', true);
      } else {
        button.prop('disabled', false);
      }
    };

    $('.select2-search-field input').focusout(function () {
      checkButton();
      window.setTimeout(function () {
        if ($('.select2-drop').is(':visible')) {
          if (!$('#select2-search-field input').is(':focus')) {
            $('#s2id_new-app-document-collections').select2('close');
            $('#s2id_new-app-edge-collections').select2('close');
            checkButton();
          }
        }
      }, 200);
    });
    $('.select2-search-field input').focusin(function () {
      if ($('.select2-drop').is(':visible')) {
        var button = $('#modalButton1');
        button.prop('disabled', true);
      }
    });
    $('#upload-foxx-zip').uploadFile({
      url: arangoHelper.databaseUrl('/_api/upload?multipart=true'),
      allowedTypes: 'zip,js',
      multiple: false,
      onSuccess: installFoxxFromZip.bind(scope)
    });
    $.get('foxxes/fishbowl', function (list) {
      var table = $('#appstore-content');
      table.html('');
      _.each(_.sortBy(list, 'name'), function (app) {
        table.append(appStoreTemplate.render(app));
      });
    }).fail(function () {
      var table = $('#appstore-content');
      table.append('<tr><td>Store is not available. ArangoDB is not able to connect to github.com</td></tr>');
    });
  };

  FoxxInstallView.prototype.install = function (callback) {
    this.reload = callback;
    this._upgrade = false;
    this._uploadData = undefined;
    delete this.mount;
    render(this, false);
    window.modalView.clearValidators();
    setMountpointValidators();
    setNewAppValidators();
  };

  FoxxInstallView.prototype.upgrade = function (mount, callback) {
    this.reload = callback;
    this._upgrade = true;
    this._uploadData = undefined;
    this.mount = mount;
    render(this, true);
    window.modalView.clearValidators();
    setNewAppValidators();
  };

  window.FoxxInstallView = FoxxInstallView;
}());

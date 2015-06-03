/*jshint browser: true */
/*global Backbone, $, window, arangoHelper, templateEngine, Joi, _*/
(function() {
  'use strict';

  window.ApplicationDetailView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('applicationDetailView.ejs'),

    events: {
      'click .open': 'openApp',
      'click .delete': 'deleteApp',
      'click #app-config': 'showConfigDialog',
      'click #app-deps': 'showDepsDialog',
      'click #app-switch-mode': 'toggleDevelopment',
      'click #app-scripts [data-script]': 'runScript',
      'click #app-tests': 'runTests',
      'click #app-upgrade': 'upgradeApp',
      'click #download-app': 'downloadApp',
      'mouseenter #app-scripts': 'showDropdown',
      'mouseleave #app-scripts': 'hideDropdown'
    },

    downloadApp: function() {
      if (!this.model.isSystem()) {
        this.model.download();
      }
    },

    upgradeApp: function() {
      var mount = this.model.get('mount');
      window.foxxInstallView.upgrade(mount, function() {
        window.App.applicationDetail(encodeURIComponent(mount));
      });
    },

    updateConfig: function() {
      this.model.getConfiguration(function () {
        $('#app-warning')[this.model.needsAttention() ? 'show' : 'hide']();
        $('#app-warning-config')[this.model.needsConfiguration() ? 'show' : 'hide']();
      }.bind(this));
    },

    updateDeps: function() {
      this.model.getDependencies(function () {
        $('#app-warning')[this.model.needsAttention() ? 'show' : 'hide']();
        $('#app-warning-deps')[this.model.hasUnconfiguredDependencies() ? 'show' : 'hide']();
      }.bind(this));
    },

    toggleDevelopment: function() {
      this.model.toggleDevelopment(!this.model.isDevelopment(), function() {
        if (this.model.isDevelopment()) {
          $('#app-switch-mode').val('Set Pro');
          $('#app-development-indicator').css('display', 'inline');
          $('#app-development-path').css('display', 'inline');
        } else {
          $('#app-switch-mode').val('Set Dev');
          $('#app-development-indicator').css('display', 'none');
          $('#app-development-path').css('display', 'none');
        }
      }.bind(this));
    },

    runScript: function(event) {
      event.preventDefault();
      var script = $(event.currentTarget).attr('data-script');
      var tableContent = [
        window.modalView.createBlobEntry(
          'app_script_arguments',
          'Script arguments',
          '', null, 'optional', false,
          [{
            rule: function (v) {
              return v && JSON.parse(v);
            },
            msg: 'Must be well-formed JSON or empty'
          }]
        )
      ];
      var buttons = [
        window.modalView.createSuccessButton('Run script', function() {
          var opts = $('#app_script_arguments').val();
          opts = opts && JSON.parse(opts);
          window.modalView.hide();
          this.model.runScript(script, opts, function (err, result) {
            var info;
            if (err) {
              info = (
                '<p>The script failed with an error'
                + (err.statusCode ? (' (HTTP ' + err.statusCode + ')') : '')
                + ':</p>'
                + '<pre>' + err.message + '</pre>'
              );
            } else if (result) {
              info = (
                '<p>Script results:</p>'
                + '<pre>' + JSON.stringify(result, null, 2) + '</pre>'
              );
            } else {
              info = '<p>The script ran successfully.</p>';
            }
            window.modalView.show(
              'modalTable.ejs',
              'Result of script "' + script + '"',
              undefined,
              undefined,
              undefined,
              info
            );
          });
        }.bind(this))
      ];
      window.modalView.show(
        'modalTable.ejs',
        'Run script "' + script + '" on "' + this.model.get('mount') + '"',
        buttons,
        tableContent
      );
    },

    runTests: function(event) {
      event.preventDefault();
      var warning = (
        '<p><strong>WARNING:</strong> Running tests may result in destructive side-effects including data loss.'
        + ' Please make sure not to run tests on a production database.</p>'
      );
      if (this.model.isDevelopment()) {
        warning += (
          '<p><strong>WARNING:</strong> This app is running in <strong>development mode</strong>.'
          + ' If any of the tests access the app"s HTTP API they may become non-deterministic.</p>'
        );
      }
      var buttons = [
        window.modalView.createSuccessButton('Run tests', function () {
          window.modalView.hide();
          this.model.runTests({reporter: 'suite'}, function (err, result) {
            window.modalView.show(
              'modalTestResults.ejs',
              'Test results',
              undefined,
              undefined,
              undefined,
              err || result
            );
          });
        }.bind(this))
      ];
      window.modalView.show(
        'modalTable.ejs',
        'Run tests for app "' + this.model.get('mount') + '"',
        buttons,
        undefined,
        undefined,
        warning
      );
    },

    render: function() {
      $(this.el).html(this.template.render({
        app: this.model,
        db: arangoHelper.currentDatabase()
      }));

      $.get(this.appUrl()).success(function () {
        $(".open", this.el).prop('disabled', false);
      }.bind(this));

      this.updateConfig();
      this.updateDeps();

      return $(this.el);
    },

    openApp: function() {
      window.open(this.appUrl(), this.model.get('title')).focus();
    },

    deleteApp: function() {
      var buttons = [
        window.modalView.createDeleteButton('Delete', function() {
          var opts = {teardown: $('#app_delete_run_teardown').is(':checked')};
          this.model.destroy(opts, function (err, result) {
            if (!err && result.error === false) {
              window.modalView.hide();
              window.App.navigate('applications', {trigger: true});
            }
          });
        }.bind(this))
      ];
      var tableContent = [
        window.modalView.createCheckboxEntry(
          'app_delete_run_teardown',
          'Run teardown?',
          true,
          'Should this app"s teardown script be executed before removing the app?',
          true
        )
      ];
      window.modalView.show(
        'modalTable.ejs',
        'Delete Foxx App mounted at "' + this.model.get('mount') + '"',
        buttons,
        tableContent,
        undefined,
        '<p>Are you sure? There is no way back...</p>',
        true
      );
    },

    appUrl: function () {
      return window.location.origin + '/_db/'
      + encodeURIComponent(arangoHelper.currentDatabase())
      + this.model.get('mount');
    },

    applyConfig: function() {
      var cfg = {};
      _.each(this.model.get('config'), function(opt, key) {
        var $el = $('#app_config_' + key);
        var val = window.arangoHelper.escapeHtml($el.val());
        if (opt.type === 'boolean' || opt.type === 'bool') {
          cfg[key] = $el.is(':checked');
          return;
        }
        if (val === '' && opt.hasOwnProperty('default')) {
          cfg[key] = opt.default;
          return;
        }
        if (opt.type === 'number') {
          cfg[key] = parseFloat(val);
        } else if (opt.type === 'integer' || opt.type === 'int') {
          cfg[key] = parseInt(val, 10);
        } else {
          cfg[key] = val;
          return;
        }
      });
      this.model.setConfiguration(cfg, function() {
        window.modalView.hide();
        this.updateConfig();
      }.bind(this));
    },

    showConfigDialog: function() {
      if (_.isEmpty(this.model.get('config'))) {
        return;
      }
      var tableContent = _.map(this.model.get('config'), function(obj, name) {
        if (obj.type === 'boolean' || obj.type === 'bool') {
          return window.modalView.createCheckboxEntry(
            'app_config_' + name,
            name,
            obj.current || false,
            obj.description,
            obj.current || false
          );
        }
        var defaultValue;
        var currentValue;
        var checks = [];
        if (obj.type === 'integer' || obj.type === 'int') {
          checks.push({
            rule: Joi.number().integer().optional().allow(''),
            msg: 'Has to be an integer.'
          });
        } else if (obj.type === 'number') {
          checks.push({
            rule: Joi.number().optional().allow(''),
            msg: 'Has to be a number.'
          });
        } else {
          checks.push({
            rule: Joi.string().optional().allow(''),
            msg: 'Has to be a string.'
          });
        }
        if (obj.default !== undefined) {
          defaultValue = String(obj.default);
        } else {
          defaultValue = '';
          checks.push({
            rule: Joi.string().required(),
            msg: 'No default found. Has to be added'
          });
        }
        if (obj.current !== undefined) {
          currentValue = String(obj.current);
        } else {
          currentValue = '';
        }
        return window.modalView.createTextEntry(
          'app_config_' + name,
          name,
          currentValue,
          obj.description,
          defaultValue,
          true,
          checks
        );
      });
      var buttons = [
        window.modalView.createSuccessButton('Apply', this.applyConfig.bind(this))
      ];
      window.modalView.show(
        'modalTable.ejs', 'Configuration', buttons, tableContent
      );

    },

    applyDeps: function() {
      var deps = {};
      _.each(this.model.get('deps'), function(title, name) {
        var $el = $('#app_deps_' + name);
        deps[name] = window.arangoHelper.escapeHtml($el.val());
      });
      this.model.setDependencies(deps, function() {
        window.modalView.hide();
        this.updateDeps();
      }.bind(this));
    },

    showDepsDialog: function() {
      if (_.isEmpty(this.model.get('deps'))) {
        return;
      }
      var tableContent = _.map(this.model.get('deps'), function(obj, name) {
        var currentValue = obj.current === undefined ? '' : String(obj.current);
        var defaultValue = '';
        var description = obj.definition;
        var checks = [
          {
            rule: Joi.string().optional().allow(''),
            msg: 'Has to be a string.'
          },
          {
            rule: Joi.string().required(),
            msg: 'This value is required.'
          }
        ];
        return window.modalView.createTextEntry(
          'app_deps_' + name,
          obj.title,
          currentValue,
          description,
          defaultValue,
          true,
          checks
        );
      });
      var buttons = [
        window.modalView.createSuccessButton('Apply', this.applyDeps.bind(this))
      ];
      window.modalView.show(
        'modalTable.ejs', 'Dependencies', buttons, tableContent
      );

    },

    showDropdown: function () {
      if (!_.isEmpty(this.model.get('scripts'))) {
        $('#scripts_dropdown').show(200);
      }
    },

    hideDropdown: function () {
      $('#scripts_dropdown').hide();
    }
  });
}());

/*jshint browser: true */
/*global Backbone, $, window, arangoHelper, templateEngine, Joi, _*/
(function() {
  "use strict";

  window.ApplicationDetailView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('applicationDetailView.ejs'),

    events: {
      'click .open': 'openApp',
      'click .delete': 'deleteApp',
      'click #configure-app': 'showConfigureDialog',
      'click #app-switch-mode': 'toggleDevelopment',
      "click #app-scripts [data-script]": "runScript",
      "click #app-tests": "runTests",
      "click #app-upgrade": "upgradeApp",
      "click #download-app": "downloadApp",
      "mouseenter #app-scripts": "showDropdown",
      "mouseleave #app-scripts": "hideDropdown"
    },

    downloadApp: function() {
      if (!this.model.isSystem()) {
        this.model.download();
      }
    },

    upgradeApp: function() {
      var mount = this.model.get("mount");
      window.foxxInstallView.upgrade(mount, function() {
        window.App.applicationDetail(encodeURIComponent(mount));
      });
    },

    updateConfig: function() {
      this.model.getConfiguration(function(cfg) {
        this._appConfig = cfg;
        if (Object.keys(this._appConfig).length === 0) {
          $("#configure-app").addClass("disabled");
        } else {
          $("#configure-app").removeClass("disabled");
        }
      }.bind(this));
    },

    toggleDevelopment: function() {
      this.model.toggleDevelopment(!this.model.isDevelopment(), function() {
        if (this.model.isDevelopment()) {
          $("#app-switch-mode").val("Set Pro");
          $("#app-development-indicator").css("display", "inline");
          $("#app-development-path").css("display", "inline");
        } else {
          $("#app-switch-mode").val("Set Dev");
          $("#app-development-indicator").css("display", "none");
          $("#app-development-path").css("display", "none");
        }
      }.bind(this));
    },

    runScript: function(event) {
      event.preventDefault();
      this.model.runScript($(event.currentTarget).attr('data-script'), function() {
      });
    },

    runTests: function(event) {
      event.preventDefault();
      var warning = (
        "WARNING: Running tests may result in destructive side-effects including data loss."
        + " Please make sure not to run tests on a production database."
        + (this.model.isDevelopment() ? (
            "\n\nWARNING: This app is running in DEVELOPMENT MODE."
            + " If any of the tests access the app's HTTP API they may become non-deterministic."
          ) : "")
        + "\n\nDo you really want to continue?"
      );
      if (!window.confirm(warning)) {
        return;
      }
      this.model.runTests({reporter: 'suite'}, function (err, result) {
        window.modalView.show(
          "modalTestResults.ejs", "Test results", [], [err || result]
        );
      }.bind(this));
    },

    render: function() {
      $(this.el).html(this.template.render({
        app: this.model,
        db: arangoHelper.currentDatabase()
      }));

      $.get(this.appUrl()).success(function () {
        $('.open', this.el).prop('disabled', false);
      }.bind(this));

      this.updateConfig();

      return $(this.el);
    },

    openApp: function() {
      window.open(this.appUrl(), this.model.get('title')).focus();
    },

    deleteApp: function() {
      var buttons = [];
      buttons.push(
        window.modalView.createDeleteButton("Delete", function() {
          this.model.destroy(function (result) {
            if (result.error === false) {
              window.modalView.hide();
              window.App.navigate("applications", { trigger: true });
            }
          });
        }.bind(this))
      );
      window.modalView.show(
        "modalDeleteConfirmation.ejs",
        "Delete Foxx App mounted at '" + this.model.get('mount') + "'",
        buttons,
        undefined,
        undefined,
        undefined,
        true
      );
    },

    appUrl: function () {
      return window.location.origin + '/_db/' +
        encodeURIComponent(arangoHelper.currentDatabase()) +
        this.model.get('mount');
    },

    readConfig: function() {
      var cfg = {};
      _.each(this._appConfig, function(opt, key) {
        var $el = $("#app_config_" + key);
        var val = window.arangoHelper.escapeHtml($el.val());
        if (opt.type === "boolean") {
          cfg[key] = $el.is(":checked");
          return;
        }
        if (val === "" && opt.hasOwnProperty("default")) {
          cfg[key] = opt.default;
          return;
        }
        if (opt.type === "number") {
          cfg[key] = parseFloat(val);
        } else if (opt.type === "integer") {
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

    showConfigureDialog: function(e) {
      if (Object.keys(this._appConfig).length === 0) {
        e.stopPropagation();
        return;
      }
      var buttons = [],
          tableContent = [],
          entry;
      _.each(this._appConfig, function(obj, name) {
        var def;
        var current;
        var check;
        switch (obj.type) {
          case "boolean":
          case "bool":
            def = obj.current || false;
            entry = window.modalView.createCheckboxEntry(
              "app_config_" + name,
              name,
              def,
              obj.description,
              def
            );
            break;
          case "integer":
            check = [{
              rule: Joi.number().integer().optional().allow(""),
              msg: "Has to be an integer."
            }];
            /* falls through */
          case "number":
            if (check === undefined) {
              check = [{
                rule: Joi.number().optional().allow(""),
                msg: "Has to be a number."
              }];
            }
            /* falls through */
          default:
            if (check === undefined) {
              check = [{
                rule: Joi.string().optional().allow(""),
                msg: "Has to be a string."
              }];
            }
            if (obj.hasOwnProperty("default")) {
              def = String(obj.default);
            } else {
              def = "";
              check.push({
                rule: Joi.string().required(),
                msg: "No default found. Has to be added"
              });
            }
            if (obj.hasOwnProperty("current")) {
              current = String(obj.current);
            } else {
              current = "";
            }
            entry = window.modalView.createTextEntry(
              "app_config_" + name,
              name,
              current,
              obj.description,
              def,
              true,
              check
            );
        }
        tableContent.push(entry);
      });
      buttons.push(
        window.modalView.createSuccessButton("Configure", this.readConfig.bind(this))
      );
      window.modalView.show(
        "modalTable.ejs", "Configure application", buttons, tableContent
      );

    },

    showDropdown: function () {
      if (this.model.get('scripts').length) {
        $("#scripts_dropdown").show(200);
      }
    },

    hideDropdown: function () {
      $("#scripts_dropdown").hide();
    }

  });
}());

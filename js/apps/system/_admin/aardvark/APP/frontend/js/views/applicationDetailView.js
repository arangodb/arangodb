/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, $, window, arangoHelper, templateEngine, Joi, _, modalDialogHelper, alert*/
(function() {

  window.ApplicationDetailView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('applicationDetailView.ejs'),

    events: {
      'click .open': 'openApp',
      'click .delete': 'deleteApp',
      'click #configure-app': 'showConfigureDialog',
      'click #app-switch-mode': 'toggleDevelopment',
      'click #app-setup': 'setup',
      'click #app-teardown': 'teardown',
      "click #app-scripts": "toggleScripts",
    },

    toggleScripts: function() {
      $("#scripts_dropdown").toggle(200);
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
        } else {
          $("#app-switch-mode").val("Set Dev");
          $("#app-development-indicator").css("display", "none");
        }
      }.bind(this));
    },

    setup: function() {
      this.model.setup(function(data) {

      });
    },

    teardown: function() {
      this.model.teardown(function(data) {

      });
    },

    render: function() {
      $(this.el).html(this.template.render({
        app: this.model,
        db: arangoHelper.currentDatabase()
      }));

      $.get(this.appUrl()).success(function () {
        $('.open', this.el).prop('disabled', false);
      }, this);

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
      this.model.setConfiguration(cfg, function(data) {
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

    }

  });
}());

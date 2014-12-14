/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, $, window, arangoHelper, templateEngine, Joi, _*/
(function() {
  "use strict";

  var splitSnakeCase = function(snakeCase) {
    var str = snakeCase.replace(/([a-z])([A-Z])/g, "$1 $2");
    str = str.replace(/([a-z])([0-9])/gi, "$1 $2");
    str = str.replace(/_+/, " ");
    return _.map(str.split(/\s+/), function(s) {
      return s.charAt(0).toUpperCase() + s.slice(1).toLowerCase();
    }).join(" ");
  };

  window.ApplicationsView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("applicationsView.ejs"),
    appStoreTemplate: templateEngine.createTemplate("applicationListView.ejs"),

    events: {
      "click #addApp"                : "createInstallModal",

      "click #checkDevel"            : "toggleDevel",
      "click #checkActive"           : "toggleActive",
      "click .checkSystem"           : "toggleSystem",
      "click #foxxToggle"            : "slideToggle",
      "click #importFoxxToggle"      : "slideToggleImport",
      "change #importFoxx"           : "uploadSetup",
      "click #confirmFoxxImport"     : "importFoxx",
      "click #installFoxxFromGithub" : "createGithubModal",
      "click .css-label"             : "checkBoxes",
      "change #appsDesc"             : "sorting"
    },


    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    uploadSetup: function (e) {
      console.log("Set allowed true");
      var files = e.target.files || e.dataTransfer.files;
      this.file = files[0];
      this.allowUpload = true;
    },

    sorting: function() {
      if ($('#appsDesc').is(":checked")) {
        this.collection.setSortingDesc(true);
      }
      else {
        this.collection.setSortingDesc(false);
      }

      this.render();
    },

    createGithubModal: function() {
      var buttons = [], tableContent = [];
      tableContent.push(
        window.modalView.createTextEntry(
          'github-url',
          'Github information',
          '',
          'Your Github link comes here: username/application-name',
          "username/application-name",
          false,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9]+[\/]/),
              msg: "No valid github link given."
            },
            {
              rule: Joi.string().required(),
              msg: "No github link given."
            }
          ]
      ));
      tableContent.push(
        window.modalView.createTextEntry(
          'github-version',
          'Version (optional)',
          '',
          'Example: v1.1.2 for version 1.1.2 - if no version is commited, master is used',
          'master',
          false,
          /[<>&'"]/
      ));
      buttons.push(
        window.modalView.createSuccessButton('Install', this.submitGithubFoxx.bind(this))
      );
      window.modalView.show(
        'modalTable.ejs', 'Install Foxx from Github', buttons, tableContent, undefined, undefined
      );
    },

    closeGithubModal: function() {
      window.modalView.hide();
    },

    submitGithubFoxx: function() {
      var name, url, version, result;

      //fetch needed information, need client side verification
      //remove name handling on server side because not needed
      name = "";
      url = $('#repository').val();
      version = $('#tag').val();

      if (version === '') {
        version = "master";
      }

      //send server req through collection
      result = this.collection.installFoxxFromGithub(url, name, version);
      if (result === true) {
        window.modalView.hide();
        this.reload();
      }
    },

    importFoxx: function() {
      var self = this;
      if (this.allowUpload) {
        this.showSpinner();
        $.ajax({
          type: "POST",
          async: false,
          url: '/_api/upload',
          data: self.file,
          processData: false,
          contentType: 'application/octet-stream',
          complete: function(res) {
            if (res.readyState === 4) {
              if (res.status === 201) {
                $.ajax({
                  type: "POST",
                  async: false,
                  url: "/_admin/aardvark/foxxes/inspect",
                  data: res.responseText,
                  contentType: "application/json"
                }).done(function(res) {
                  $.ajax({
                    type: "POST",
                    async: false,
                    url: '/_admin/foxx/fetch',
                    data: JSON.stringify({
                      name: res.name,
                      version: res.version,
                      filename: res.filename
                    }),
                    processData: false
                  }).done(function () {
                    self.reload();
                  }).fail(function (err) {
                    self.hideSpinner();
                    var error = JSON.parse(err.responseText);
                    arangoHelper.arangoError("Error: " + error.errorMessage);
                  });
                }).fail(function(err) {
                  self.hideSpinner();
                  var error = JSON.parse(err.responseText);
                  arangoHelper.arangoError("Error: " + error.error);
                });
                delete self.file;
                self.allowUpload = false;
                self.hideSpinner();
                self.hideImportModal();
                return;
              }
            }
            self.hideSpinner();
            arangoHelper.arangoError("Upload error");
          }
        });
      }
    },

    showSpinner: function() {
      $('#uploadIndicator').show();
    },

    hideSpinner: function() {
      $('#uploadIndicator').hide();
    },

    toggleDevel: function() {
      var self = this;
      this._showDevel = !this._showDevel;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
      });
    },

    toggleActive: function() {
      var self = this;
      this._showActive = !this._showActive;
      _.each(this._installedSubViews, function(v) {
        v.toggle("active", self._showActive);
      });
    },

    toggleSystem: function() {
      this._showSystem = !this._showSystem;
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("system", self._showSystem);
      });
      this.createSubViews();
      this.renderSubViews();
    },

    hideImportModal: function() {
      $('#foxxDropdownImport').hide();
    },

    hideSettingsModal: function() {
      $('#foxxDropdownOut').hide();
    },

    slideToggleImport: function() {
      $('#foxxToggle').removeClass('activated');
      $('#importFoxxToggle').toggleClass('activated');
      $('#foxxDropdownImport').slideToggle(200);
      this.hideSettingsModal();
    },

    slideToggle: function() {
      //apply sorting to checkboxes
      $('#appsDesc').attr('checked', this.collection.sortOptions.desc);

      $('#importFoxxToggle').removeClass('activated');
      $('#foxxToggle').toggleClass('activated');
      $('#foxxDropdownOut').slideToggle(200);
      this.hideImportModal();
    },

    reload: function() {
      var self = this;

      // unbind and remove any unused views
      _.each(this._installedSubViews, function (v) {
        v.undelegateEvents();
      });
      _.each(this._availableSubViews, function (v) {
        v.undelegateEvents();
      });

      this.collection.fetch({
        success: function() {
          self.createSubViews();
          self.render();
        }
      });
    },

    createSubViews: function() {
      var self = this;
      this._installedSubViews = { };
      this._availableSubViews = { };

      self.collection.each(function (foxx) {
        if (foxx.get('isSystem') && ! self._showSystem) {
          return;
        }
        var subView;
        if (foxx.get("type") === "app") {
          subView = new window.FoxxInstalledView({
            model: foxx,
            appsView: self
          });
          self._availableSubViews[foxx.get('_id')] = subView;
        } else if (foxx.get("type") === "mount") {
          subView = new window.FoxxActiveView({
            model: foxx,
            appsView: self
          });
          self._installedSubViews[foxx.get('_id')] = subView;
        }
      });
    },

    initialize: function() {
      this._installedSubViews = {};
      this._availableSubViews = {};
      this._showDevel = true;
      this._showActive = true;
      this._showSystem = true;
      this.reload();
    },

    render: function() {
      this.collection.sort();

      $(this.el).html(this.template.render({}));
      this.renderSubViews();
      this.delegateEvents();
      $('#checkActive').attr('checked', this._showActive);
      $('#checkDevel').attr('checked', this._showDevel);
      $('.checkSystem').attr('checked', this._showSystem);
      
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
        v.toggle("active", self._showActive);
        v.toggle("system", self._showSystem);
      });

      arangoHelper.fixTooltips("icon_arangodb", "left");
      return this;
    },

    renderSubViews: function () {
      _.each(this._installedSubViews, function (v) {
        $("#installedList").append(v.render());
      });

      var versions = { };
      _.each(this._availableSubViews, function (v) {
        var name = v.model.get("name");

        //look which installed apps have multiple versions
        if (versions[name]) {
          versions[name].counter++;
          versions[name].versions.push(v.model.get("version"));
        }
        else {
          versions[name] = {
            counter: 1,
            versions: [v.model.get("version")]
          };
        }
      });

      _.each(this._availableSubViews, function (v) {
        var name = v.model.get("name"),
        version = v.model.get("version");
        if (versions[name].counter > 1 ) {
          //here comes special render for multiple versions view

          var highestVersion = "0.0.0";
          var selectOptions = [];

          _.each(versions[name].versions, function(x) {
            selectOptions.push({
              value: x,
              label: x
            });
            if (x > highestVersion) {
              highestVersion = x;
            }
          });

          if (version === highestVersion) {
            v.model.set("highestVersion", highestVersion);
            v.model.set("versions", versions[name].versions);
            v.model.set("selectOptions", selectOptions);

            $("#availableList").append(v.render());
          }
        }
        else {
          $("#availableList").append(v.render());
        }
      });
    },

    ///// NEW CODE 

    installFoxxFromZip: function() {
      var self = this;
      if (this.allowUpload) {
        this.showSpinner();
        $.ajax({
          type: "POST",
          async: false,
          url: '/_api/upload',
          data: self.file,
          processData: false,
          contentType: 'application/octet-stream',
          complete: function(res) {
            if (res.readyState === 4) {
              if (res.status === 201) {
                $.ajax({
                  type: "POST",
                  async: false,
                  url: "/_admin/aardvark/foxxes/inspect",
                  data: res.responseText,
                  contentType: "application/json"
                }).done(function(res) {
                  console.log("Peter2", res);
                  $.ajax({
                    type: "POST",
                    async: false,
                    url: '/_admin/foxx/fetch',
                    data: JSON.stringify({
                      name: res.name,
                      version: res.version,
                      filename: res.filename
                    }),
                    processData: false
                  }).done(function (result) {
                    console.log("Peter", result);
                    if (result.error === false) {
                      window.modalView.hide();
                      self.showConfigureDialog(res.configuration, res.name, res.version);
                    }
                  }).fail(function (err) {
                    self.hideSpinner();
                    var error = JSON.parse(err.responseText);
                    arangoHelper.arangoError("Error: " + error.errorMessage);
                  });
                }).fail(function(err) {
                  self.hideSpinner();
                  var error = JSON.parse(err.responseText);
                  arangoHelper.arangoError("Error: " + error.error);
                });
                delete self.file;
                self.allowUpload = false;
                self.hideSpinner();
                self.hideImportModal();
                return;
              }
            }
            self.hideSpinner();
            arangoHelper.arangoError("Upload error");
          }
        });
      }
    },

    installFoxxFromStore: function(e) {
      var toInstall = $(e.currentTarget).attr("appId");
      var version = $(e.currentTarget).attr("appVersion");
      var self = this;
      $.post("foxxes/fishbowlinstall", JSON.stringify({
        name: toInstall,
        version: version
      }), function(result) {
        if (result.error === false) {
          window.modalView.hide();
          self.showConfigureDialog(result.configuration, result.name, result.version);
        }
      });
    },

    installFoxxFromGithub: function() {
      var name, url, version, result;

      //fetch needed information, need client side verification
      //remove name handling on server side because not needed
      name = "";
      url = $('#repository').val();
      version = $('#tag').val();

      if (version === '') {
        version = "master";
      }

      //send server req through collection
      result = this.collection.installFoxxFromGithub(url, name, version);
      if (result.error === false) {
        window.modalView.hide();
        this.showConfigureDialog(result.configuration, result.name, result.version);
      }
    },

    showConfigureDialog: function(config, name, version) {
      var buttons = [],
          tableContent = [],
          entry;
      tableContent.push(
        window.modalView.createTextEntry(
          "mount-point",
          "Mount",
          "",
          "The path the App will be mounted. Has to start with /. Is not allowed to start with /_",
          "/my/app",
          true,
          //TODO nochmal schauen
          [
            {
              rule: Joi.string().required(),
              msg: "No mountpoint given."
            },
            {
              rule: Joi.string().regex(/^\/[^_]/),
              msg: "Mountpoints with _ are reserved for internal use."
            },
            {
              rule: Joi.string().regex(/^(\/[a-zA-Z0-9_\-%]+)+$/),
              msg: "Mountpoints have to start with / and can only contain [a-zA-Z0-9_-%]"
            }
          ]
        )
      );
      _.each(config, function(obj, name) {
        var def;
        var check;
        switch (obj.type) {
          case "boolean":
          case "bool":
            def = obj.default || false;
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
              rule: Joi.number().integer(),
              msg: "Has to be an integer."
            }];
          default:
            if (check === undefined) {
              check = [{
                rule: Joi.string(),
                msg: "Has to be non-empty."
              }];
            }
            if (obj.default === undefined) {
              def = "";
            } else {
              def = String(obj.default);
            }
            entry = window.modalView.createTextEntry(
              "app_config_" + name,
              name,
              def,
              obj.description,
              def,
              true,
              check
            );
        }
        tableContent.push(entry);
      });
      buttons.push(
        window.modalView.createSuccessButton("Configure", this.mountFoxx.bind(this, config, name, version))
      );
      window.modalView.show(
        "modalTable.ejs", "Configure Application", buttons, tableContent
      );

    },

    mountFoxx: function(config, name, version) {
      var cfg = {};
      try {
        _.each(config, function(opt, key) {
          var $el = $("#app_config_" + key);
          var val = $el.val();
          if (opt.type === "boolean") {
            cfg[key] = $el.is(":checked");
            return;
          }
          if (val === "" && !opt.hasOwnProperty("default")) {
            throw new SyntaxError(
              "Must specify value for field \"" +
                (opt.label || splitSnakeCase(key)) +
                "\"!"
            );
          }
          if (opt.type === "number") {
            cfg[key] = parseFloat(val);
          } else if (opt.type === "integer") {
            cfg[key] = parseInt(val, 10);
          } else {
            cfg[key] = val;
            return;
          }
          if (_.isNaN(cfg[key])) {
            throw new SyntaxError(
              "Invalid value for field \"" +
                (opt.label || splitSnakeCase(key)) +
                "\"!"
            );
          }
          if (opt.type === "integer" && cfg[key] !== Math.floor(parseFloat(val))) {
            throw new SyntaxError(
              "Expected non-decimal value in field \"" +
                (opt.label || splitSnakeCase(key)) +
                "\"!"
            );
          }
        });
      } catch (err) {
        if (err instanceof SyntaxError) {
          alert(err.message);
          return false;
        }
        throw err;
      }
      var self = this;
      this.collection.create({
        mount: $("#mount-point").val(),
        name: name,
        version: version,
        options: {
          configuration: cfg
        }
      },
      {
        success: function() {
          window.modalView.hide();
          self.reload();
        },
        error: function(e, info) {
          if (info.responseText.indexOf("already used by") > -1) {
            alert("Mount-Path already in use.");
          } else if (info.responseText.indexOf("app is not defined") > -1) {
            //temp ignore this message, fix needs to be server-side
            window.modalView.hide();
            self.reload();
          } else {
            alert(info.statusText);
          }
        }
      });
    },

    generateNewFoxxApp: function() {
      var info = {
        name: $("#new-app-name").val(),
        collectionNames: _.pluck($('#new-app-collections').select2("data"), "text"),
        authenticated: $("#new-app-name").val(),
        author: $("#new-app-author").val(),
        license: $("#new-app-license").val(),
        description: $("#new-app-description").val()
      };
      console.log(info);
      $.post("templates/generate", info, function(a) {
        alert("GENERATOOOO!", a);
      });
    },

    addAppAction: function() {
      var openTab = $(".modal-body .tab-pane.active").attr("id");
      switch (openTab) {
        case "newApp":
          this.generateNewFoxxApp();
          break;
        case "github":
          this.installFoxxFromGithub();
          break;
        case "zip":
          this.installFoxxFromZip();
          break;
        default:
      }
    },

    switchModalButton: function(event) {
      var openTab = $(event.currentTarget).attr("href").substr(1);
      var button = $("#modalButton1");
      switch (openTab) {
        case "newApp":
          button.html("Generate");
          button.prop("disabled", false);
          break;
        case "appstore":
          button.html("Install");
          button.prop("disabled", true);
          break;
        case "github":
        case "zip":
          button.html("Install");
          button.prop("disabled", false);
          break;
        default:
      }
    },

    createInstallModal: function(event) {
      event.preventDefault();
      var buttons = [];
      var modalEvents = {
        "click #infoTab a": this.switchModalButton.bind(this),
        "click .install-app"         : this.installFoxxFromStore.bind(this),
        "change #zip-file"             : this.uploadSetup.bind(this)
      };
      buttons.push(
        window.modalView.createSuccessButton("Generate", this.addAppAction.bind(this))
      );
      window.modalView.show(
        "modalApplicationMount.ejs",
        "Install Application",
        buttons,
        undefined,
        undefined,
        modalEvents
      );
      $("#new-app-collections").select2({
        tags: [],
        showSearchBox: false,
        minimumResultsForSearch: -1,
        width: "336px"
      });
      var listTempl = this.appStoreTemplate;
      $.get("foxxes/fishbowl", function(list) {
        var table = $("#appstore-content");
        _.each(_.sortBy(list, "name"), function(app) {
          table.append(listTempl.render(app));
        });
      });
    }

  });

}());

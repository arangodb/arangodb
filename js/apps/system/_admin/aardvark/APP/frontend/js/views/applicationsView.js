/*jshint browser: true */
/*global require, Backbone, $, window, arangoHelper, templateEngine, Joi, alert, _*/
(function() {
  "use strict";

  var errors = require("internal").errors;

  window.ApplicationsView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("applicationsView.ejs"),
    appStoreTemplate: templateEngine.createTemplate("applicationListView.ejs"),

    events: {
      "click #addApp"                : "createInstallModal",
      "click #foxxToggle"            : "slideToggle",
      "click #checkDevel"            : "toggleDevel",
      "click #checkProduction"       : "toggleProduction",
      "click #checkSystem"           : "toggleSystem",
      "click .css-label"             : "checkBoxes"
    },


    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    toggleDevel: function() {
      var self = this;
      this._showDevel = !this._showDevel;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
      });
    },

    toggleProduction: function() {
      var self = this;
      this._showProd = !this._showProd;
      _.each(this._installedSubViews, function(v) {
        v.toggle("production", self._showProd);
      });
    },

    toggleSystem: function() {
      this._showSystem = !this._showSystem;
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("system", self._showSystem);
      });
    },

    reload: function() {
      var self = this;

      // unbind and remove any unused views
      _.each(this._installedSubViews, function (v) {
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

      self.collection.each(function (foxx) {
        var subView = new window.FoxxActiveView({
          model: foxx,
          appsView: self
        });
        self._installedSubViews[foxx.get('mount')] = subView;
      });
    },

    initialize: function() {
      this._installedSubViews = {};
      this._showDevel = true;
      this._showProd = true;
      this._showSystem = false;
      this.reload();
    },

    slideToggle: function() {
      $('#foxxToggle').toggleClass('activated');
      $('#foxxDropdownOut').slideToggle(200);
    },

    installFoxxFromStore: function(e) {
      var mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      var toInstall = $(e.currentTarget).attr("appId");
      var version = $(e.currentTarget).attr("appVersion");
      this.collection.installFromStore({name: toInstall, version: version}, mount, this.installCallback.bind(this));
    },

    installFoxxFromZip: function(files, data) {
      var mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      this.collection.installFromZip(data.filename, mount, this.installCallback.bind(this));
    },

    installCallback: function(result) {
      if (result.error === false) {
        window.modalView.hide();
        this.reload();
      } else {
        // TODO Error handling properly!
        switch(result.errorNum) {
          case errors.ERROR_APPLICATION_DOWNLOAD_FAILED.code:
            alert("Unable to download application from the given repository.");
            break;
          default:
            alert("Error: " + result.errorNum + ". " + result.errorMessage);
        }
      }
    },

    installFoxxFromGithub: function() {
      var url, version, mount;

      //fetch needed information, need client side verification
      //remove name handling on server side because not needed
      mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      url = window.arangoHelper.escapeHtml($('#repository').val());
      version = window.arangoHelper.escapeHtml($('#tag').val());

      if (version === '') {
        version = "master";
      }
      var info = {
        url: window.arangoHelper.escapeHtml($('#repository').val()),
        version: window.arangoHelper.escapeHtml($('#tag').val())
      };

      try {
        Joi.assert(url, Joi.string().regex(/^[a-zA-Z0-9_\-]+\/[a-zA-Z0-9_\-]+$/));
      } catch (e) {
        return;
      }
      //send server req through collection
      this.collection.installFromGithub(info, mount, this.installCallback.bind(this));
    },

    /* potential error handling
    if (info.responseText.indexOf("already used by") > -1) {
      alert("Mount path already in use.");
    } else if (info.responseText.indexOf("app is not defined") > -1) {
      //temp ignore this message, fix needs to be server-side
      window.modalView.hide();
      self.reload();
    } else {
      alert(info.statusText);
    }
    */


    /// NEW CODE

    generateNewFoxxApp: function() {
      var mount;
      mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      var info = {
        name: window.arangoHelper.escapeHtml($("#new-app-name").val()),
        collectionNames: _.map($('#new-app-collections').select2("data"), function(d) {
          return window.arangoHelper.escapeHtml(d.text);
        }),
//        authenticated: window.arangoHelper.escapeHtml($("#new-app-name").val()),
        author: window.arangoHelper.escapeHtml($("#new-app-author").val()),
        license: window.arangoHelper.escapeHtml($("#new-app-license").val()),
        description: window.arangoHelper.escapeHtml($("#new-app-description").val())
      };
      this.collection.generate(info, mount, this.installCallback.bind(this));
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
          // this.installFoxxFromZip();
          break;
        default:
      }
    },

    setMountpointValidators: function() {
      window.modalView.clearValidators();
      window.modalView.modalBindValidation({
        id: "new-app-mount",
        validateInput: function() {
          return [
            {
              rule: Joi.string().regex(/^(\/(APP[^\/]+|(?!APP)[a-zA-Z0-9_\-%]+))+$/i),
              msg: "May not contain /APP"
            },
            {
              rule: Joi.string().regex(/^(\/[a-zA-Z0-9_\-%]+)+$/),
              msg: "Can only contain [a-zA-Z0-9_-%]"
            },
            {
              rule: Joi.string().regex(/^\/[^_]/),
              msg: "Mountpoints with _ are reserved for internal use"
            },
            {
              rule: Joi.string().regex(/[^\/]$/),
              msg: "May not end with /"
            },
            {
              rule: Joi.string().regex(/^\//),
              msg: "Has to start with /"
            },
            {
              rule: Joi.string().required().min(2),
              msg: "Has to be non-empty"
            },
          ];
        }
      });
    },

    setGithubValidators: function() {
      window.modalView.modalBindValidation({
        id: "repository",
        validateInput: function() {
          return [
            {
              rule: Joi.string().required().regex(/^[a-zA-Z0-9_\-]+\/[a-zA-Z0-9_\-]+$/),
              msg: "No valid github account and repository."
            }
          ];
        }
      });
    },

    setNewAppValidators: function() {
      
      window.modalView.modalBindValidation({
        id: "new-app-author",
        validateInput: function() {
          return [
            {
              rule: Joi.string().required().min(1),
              msg: "Has to be non empty."
            }
          ];
        }
      });

      window.modalView.modalBindValidation({
        id: "new-app-name",
        validateInput: function() {
          return [
            {
              rule: Joi.string().required().regex(/^[a-zA-Z\-_][a-zA-Z0-9\-_]*$/),
              msg: "Can only contain a to z, A to Z, 0-9, '-' and '_'."
            }
          ];
        }
      });

      window.modalView.modalBindValidation({
        id: "new-app-description",
        validateInput: function() {
          return [
            {
              rule: Joi.string().required().min(1),
              msg: "Has to be non empty."
            }
          ];
        }
      });

      window.modalView.modalBindValidation({
        id: "new-app-license",
        validateInput: function() {
          return [
            {
              rule: Joi.string().required().regex(/^[a-zA-Z0-9 \.,;\-]+$/),
              msg: "Has to be non empty."
            }
          ];
        }
      });
      window.modalView.modalTestAll();
    },

    switchModalButton: function(event) {
      window.modalView.clearValidators();
      var openTab = $(event.currentTarget).attr("href").substr(1);
      var button = $("#modalButton1");
      this.setMountpointValidators();
      switch (openTab) {
        case "newApp":
          button.html("Generate");
          button.prop("disabled", false);
          this.setNewAppValidators();
          break;
        case "appstore":
          button.html("Install");
          button.prop("disabled", true);
          break;
        case "github":
          this.setGithubValidators();
          button.html("Install");
          button.prop("disabled", false);
          break;
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
        "click #infoTab a"   : this.switchModalButton.bind(this),
        "click .install-app" : this.installFoxxFromStore.bind(this)
      };
      buttons.push(
        window.modalView.createSuccessButton("Generate", this.addAppAction.bind(this))
      );
      window.modalView.show(
        "modalApplicationMount.ejs",
        "Install application",
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
      $("#upload-foxx-zip").uploadFile({
        url: "/_api/upload?multipart=true",
        allowedTypes: "zip",
        onSuccess: this.installFoxxFromZip.bind(this)
      });
      var listTempl = this.appStoreTemplate;
      $.get("foxxes/fishbowl", function(list) {
        var table = $("#appstore-content");
        _.each(_.sortBy(list, "name"), function(app) {
          table.append(listTempl.render(app));
        });
      }).fail(function() {
        var table = $("#appstore-content");
        table.append("<tr><td>Store is not available. ArangoDB is not able to connect to github.com</td></tr>");
      });
      this.setMountpointValidators();
      this.setNewAppValidators();
    },

    render: function() {
      this.collection.sort();

      $(this.el).html(this.template.render({}));
      _.each(this._installedSubViews, function (v) {
        $("#installedList").append(v.render());
      });
      this.delegateEvents();
      $('#checkDevel').attr('checked', this._showDevel);
      $('#checkProduction').attr('checked', this._showProd);
      $('#checkSystem').attr('checked', this._showSystem);
      
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
        v.toggle("system", self._showSystem);
      });

      arangoHelper.fixTooltips("icon_arangodb", "left");
      return this;
    }

  });

  /* Info for mountpoint
   *
   *
      window.modalView.createTextEntry(
        "mount-point",
        "Mount",
        "",
        "The path the app will be mounted. Has to start with /. Is not allowed to start with /_",
        "/my/app",
        true,
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
   */
}());

/*jshint browser: true */
/*global require, $, Joi, _, alert, templateEngine*/
(function() {
  "use strict";

  var errors = require("internal").errors;
  var appStoreTemplate = templateEngine.createTemplate("applicationListView.ejs");

  var FoxxInstallView = function(opts) {
    this.collection = opts.collection;
  };

  var installCallback = function(result) {
    if (result.error === false) {
      this.collection.fetch({ async: false });
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
  };

  var setMountpointValidators = function() {
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
  };

  var setGithubValidators = function() {
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
  };

  var setNewAppValidators = function() {
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
  };

  var switchModalButton = function(event) {
    window.modalView.clearValidators();
    var openTab = $(event.currentTarget).attr("href").substr(1);
    var button = $("#modalButton1");
    if (!this._upgrade) {
      setMountpointValidators();
    }
    switch (openTab) {
      case "newApp":
        button.html("Generate");
        button.prop("disabled", false);
        setNewAppValidators();
        break;
      case "appstore":
        button.html("Install");
        button.prop("disabled", true);
        break;
      case "github":
        setGithubValidators();
        button.html("Install");
        button.prop("disabled", false);
        break;
      case "zip":
        button.html("Install");
        button.prop("disabled", false);
        break;
      default:
    }
    
    if (! button.prop("disabled") && ! window.modalView.modalTestAll()) {
      // trigger the validation so the "ok" button has the correct state
      button.prop("disabled", true);
    }
  };

  var installFoxxFromStore = function(e) {
    if (window.modalView.modalTestAll()) {
      var mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop("checked");
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
      var toInstall = $(e.currentTarget).attr("appId");
      var version = $(e.currentTarget).attr("appVersion");
      if (flag !== undefined) {
        this.collection.installFromStore({name: toInstall, version: version}, mount, installCallback.bind(this), flag);
      } else {
        this.collection.installFromStore({name: toInstall, version: version}, mount, installCallback.bind(this));
      }
    }
  };

  var installFoxxFromZip = function(files, data) {
    if (data === undefined) {
      data = this._uploadData;
    }
    else {
      this._uploadData = data;
    }
    if (data && window.modalView.modalTestAll()) {
      var mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop("checked");
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
      if (flag !== undefined) {
        this.collection.installFromZip(data.filename, mount, installCallback.bind(this), flag);
      } else {
        this.collection.installFromZip(data.filename, mount, installCallback.bind(this));
      }
    }
  };


  var installFoxxFromGithub = function() {
    if (window.modalView.modalTestAll()) {
      var url, version, mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop("checked");
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
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
      if (flag !== undefined) {
        this.collection.installFromGithub(info, mount, installCallback.bind(this), flag);
      } else {
        this.collection.installFromGithub(info, mount, installCallback.bind(this));

      }
    }
  };

  var generateNewFoxxApp = function() {
    if (window.modalView.modalTestAll()) {
      var mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop("checked");
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
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
      if (flag !== undefined) {
        this.collection.generate(info, mount, installCallback.bind(this), flag);
      } else {
        this.collection.generate(info, mount, installCallback.bind(this));
      }
    }
  };

  var addAppAction = function() {
    var openTab = $(".modal-body .tab-pane.active").attr("id");
    switch (openTab) {
      case "newApp":
        generateNewFoxxApp.apply(this);
        break;
      case "github":
        installFoxxFromGithub.apply(this);
        break;
      case "zip":
        installFoxxFromZip.apply(this);
        break;
      default:
    }
  };

  var render = function(scope, upgrade) {
    var buttons = [];
    var modalEvents = {
      "click #infoTab a"   : switchModalButton.bind(scope),
      "click .install-app" : installFoxxFromStore.bind(scope)
    };
    buttons.push(
      window.modalView.createSuccessButton("Generate", addAppAction.bind(scope))
    );
    window.modalView.show(
      "modalApplicationMount.ejs",
      "Install application",
      buttons,
      upgrade,
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
      multiple: false,
      onSuccess: installFoxxFromZip.bind(scope)
    });
    $.get("foxxes/fishbowl", function(list) {
      var table = $("#appstore-content");
      _.each(_.sortBy(list, "name"), function(app) {
        table.append(appStoreTemplate.render(app));
      });
    }).fail(function() {
      var table = $("#appstore-content");
      table.append("<tr><td>Store is not available. ArangoDB is not able to connect to github.com</td></tr>");
    });
  };

  FoxxInstallView.prototype.install = function(callback) {
    this.reload = callback;
    this._upgrade = false;
    this._uploadData = undefined;
    delete this.mount;
    render(this, false);
    window.modalView.clearValidators();
    setMountpointValidators();
    setNewAppValidators();

  };

  FoxxInstallView.prototype.upgrade = function(mount, callback) {
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

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, _, templateEngine, alert*/

(function() {
  "use strict";
  function splitSnakeCase(snakeCase) {
    var str = snakeCase.replace(/([a-z])([A-Z])/g, "$1 $2");
    str = str.replace(/([a-z])([0-9])/gi, "$1 $2");
    str = str.replace(/_+/, " ");
    return _.map(str.split(/\s+/), function(s) {
      return s.charAt(0).toUpperCase() + s.slice(1).toLowerCase();
    }).join(" ");
  }
  window.FoxxInstalledView = Backbone.View.extend({
    tagName: "div",
    className: "tile",
    template: templateEngine.createTemplate("foxxInstalledView.ejs"),

    events: {
      //"click .install": "installDialog",
      //"click .purge": "removeDialog",
      "click .icon_arangodb_settings2": "infoDialog"
    },

    renderVersion: function(e) {
      var name = this.model.get("name"),
      selectOptions = this.model.get("selectOptions"),
      versionToRender = $("#"+name+"Select").val();
      this.model.set("activeVersion", versionToRender);

      var toRender = this.model.collection.findWhere({
        name: name,
        version: versionToRender
      });

    },

    initialize: function(){
      _.bindAll(this, "render");
      var buttonConfig = [
        window.modalView.createSuccessButton(
          "Install", this.install.bind(this)
        )
      ];
      var buttonPurgeConfig = [
        window.modalView.createDeleteButton(
          "Remove", this.confirmRemovalSingle.bind(this)
        )
      ];
      var buttonInfoConfig = [
        window.modalView.createDeleteButton(
          "Remove", this.confirmRemovalSingle.bind(this)
        ),
        window.modalView.createSuccessButton(
          "Install", this.installDialog.bind(this)
        )
      ];
      var buttonInfoConfigUpdate = [
        window.modalView.createDeleteButton(
          "Remove", this.confirmRemovalSingle.bind(this)
        ),
        window.modalView.createSuccessButton(
          "Update", this.update.bind(this)
        ),
        window.modalView.createSuccessButton(
          "Install", this.installDialog.bind(this)
        )
      ];
      var buttonInfoMultipleVersionsConfig = [
        window.modalView.createDeleteButton(
          "Remove All", this.confirmRemovalAll.bind(this)
        ),
        window.modalView.createDeleteButton(
          "Remove", this.confirmRemovalSingle.bind(this)
        ),
        window.modalView.createSuccessButton(
          "Install", this.installDialog.bind(this)
        )
      ];
      var buttonInfoMultipleVersionsConfigUpdate = [
        window.modalView.createDeleteButton(
          "Remove All", this.confirmRemovalAll.bind(this)
        ),
        window.modalView.createDeleteButton(
          "Remove", this.confirmRemovalSingle.bind(this)
        ),
        window.modalView.createSuccessButton(
          "Update", this.update.bind(this)
        ),
        window.modalView.createSuccessButton(
          "Install", this.installDialog.bind(this)
        )
      ];
      this.showMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Install Application",
        buttonConfig
      );
      this.showInfoMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Application Settings",
        buttonInfoConfig
      );
      this.showInfoModUpdate = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Application Settings",
        buttonInfoConfigUpdate
      );
      this.showInfoMultipleVersionsMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Application Settings",
        buttonInfoMultipleVersionsConfig
      );
      this.showInfoMultipleVersionsModUpdate = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Application Settings",
        buttonInfoMultipleVersionsConfigUpdate
      );
      this.showPurgeMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Remove Application",
        buttonPurgeConfig
      );
      this.appsView = this.options.appsView;
    },

    //add select here too
    fillPurgeValues: function(mountinfo) {
      var list = [],
          isSystem,
          mountvars = [];
      if (this.model.get("isSystem")) {
        isSystem = "Yes";
      } else {
        isSystem = "No";
      }
      list.push(window.modalView.createReadOnlyEntry(
        "id_name", "Name", this.model.get("name")
      ));
      if (this.model.get("selectOptions")) {
        list.push(window.modalView.createSelectEntry(
          "foxx-version",
          "Version",
          "",
          "Select version of the Foxx application",
          this.model.get("selectOptions")
        ));
      }
      else {
        list.push(window.modalView.createReadOnlyEntry(
          "id_version", "Version", this.model.get("version")
        ));
      }
      list.push(window.modalView.createReadOnlyEntry(
        "id_system", "System", isSystem
      ));
      list.push(window.modalView.createReadOnlyEntry(
        "id_just_info", "Info", "All mount points will be unmounted and removed"
      ));
      if (mountinfo) {
        _.each(mountinfo, function(m) {
          mountvars.push(m.mount);
        });

        list.push(window.modalView.createReadOnlyEntry(
          "id_just_mountinfo", "Mount-points", JSON.stringify(mountvars)
        ));
      }
      return list;
    },

    fillValues: function() {
      var list = [],
          isSystem;
      if (this.model.get("isSystem")) {
        isSystem = "Yes";
      } else {
        isSystem = "No";
      }
      list.push(window.modalView.createReadOnlyEntry(
        "id_name", "Name", this.model.get("name")
      ));
      list.push(window.modalView.createTextEntry(
        "mount-point", "Mount", "/" + this.model.get("name"),
        "The path where the app can be reached.",
        "mount-path",
        true
      ));

      var configs = this.model.get("manifest").configuration;
      if (configs && _.keys(configs).length) {
        _.each(_.keys(configs), function(key) {
          var opt = configs[key];
          var prettyKey = opt.label || splitSnakeCase(key);
          if (opt.type === "boolean") {
            list.push(window.modalView.createCheckboxEntry(
              "foxx_configs_" + key,
              prettyKey,
              true,
              opt.description,
              opt.default
            ));
          } else {
            list.push(window.modalView.createTextEntry(
              "foxx_configs_" + key,
              prettyKey,
              opt.default === undefined ? "" : opt.default,
              opt.description,
              splitSnakeCase(key).toLowerCase().replace(/\s+/g, "-"),
              opt.default === undefined
            ));
          }
        });
      }
      if (this.model.get("selectOptions")) {
        list.push(window.modalView.createSelectEntry(
          "foxx-version",
          "Version",
          "",
          "Select version of the Foxx application",
          this.model.get("selectOptions")
        ));
      }
      else {
        list.push(window.modalView.createReadOnlyEntry(
          "id_version", "Version", this.model.get("version")
        ));
      }
      list.push(window.modalView.createReadOnlyEntry(
        "id_system", "System", isSystem
      ));
      return list;
    },

    fillInfoValues: function() {
      var list = [],
          isSystem;
      if (this.model.get("isSystem")) {
        isSystem = "Yes";
      } else {
        isSystem = "No";
      }
      list.push(window.modalView.createReadOnlyEntry(
        "id_name", "Name", this.model.get("name")
      ));
      list.push(window.modalView.createReadOnlyEntry(
        "id_description", "Description", this.model.get("description")
      ));
      list.push(window.modalView.createReadOnlyEntry(
        "id_author", "Author", this.model.get("author")
      ));
      if (this.model.get("selectOptions")) {
        list.push(window.modalView.createSelectEntry(
          "foxx-version",
          "Version",
          "",
          "Select version of the Foxx application",
          this.model.get("selectOptions")
        ));
      }
      else {
        list.push(window.modalView.createReadOnlyEntry(
          "id_version", "Version", this.model.get("version")
        ));
      }
      list.push(window.modalView.createReadOnlyEntry(
        "id_system", "System", isSystem
      ));
      if (this.model.get("git")) {
        list.push(window.modalView.createReadOnlyEntry(
          "id_git", "Git", this.model.get("git")
        ));
      }
      return list;
    },

    removeDialog: function(event) {
      event.stopPropagation();
      var name = this.model.get("name");
      var mountinfo = this.model.collection.mountInfo(name);
      this.showPurgeMod(this.fillPurgeValues(mountinfo));
    },

    confirmRemovalSingle: function(event) {

      var version, self = this;

      if (this.model.get("versions")) {
        version = $("#foxx-version").val();
      }
      else {
        version = this.model.get("version");
      }

      //find correct app install version
      var toDelete = this.model.collection.findWhere({
        name: self.model.get("name"),
        version: version
      });

      var name = toDelete.get("app");
      var result = toDelete.collection.purgeFoxx(name);

      if (result === true) {
        window.modalView.hide();
        window.App.applicationsView.reload();
      }
    },

    confirmRemovalAll: function(event) {
      var name = this.model.get("name");
      var result = this.model.collection.purgeAllFoxxes(name);

      if (result === true) {
        window.modalView.hide();
        window.App.applicationsView.reload();
      }
    },

    infoDialog: function(event) {
      var name = this.model.get("name"),
      mountinfo = this.model.collection.gitInfo(name),
      isGit;

      if (mountinfo.git === true) {
        this.model.set("isGit", mountinfo.git);
        this.model.set("gitUrl", mountinfo.url);
      }

      isGit = this.model.get("isGit");

      event.stopPropagation();

      this[
        this.model.get("versions")
        ? (isGit ? 'showInfoMultipleVersionsModUpdate' : 'showInfoMultipleVersionsMod')
        : (isGit ? 'showInfoModUpdate' : 'showInfoMod')
      ](this.fillInfoValues());

      this.selectHighestVersion();
    },

    installDialog: function(event) {

      var currentVersion = $(".modalSelect").val();

      window.modalView.hide();
      event.stopPropagation();
      this.showMod(this.fillValues());

      this.selectSpecificVersion(currentVersion);
    },

    update: function() {
      var url = this.model.get("gitUrl"),
      version = "master",
      name = "",
      result;

      if (url === undefined || url === "") {
        // if no git is defined
        return;
      }

      result = this.model.collection.installFoxxFromGithub(url, name, version);
      if (result === true) {
        window.modalView.hide();
        window.App.applicationsView.reload();
      }
    },

    install: function() {
      var mountPoint = $("#mount-point").val(),
        version = "",
        self = this;
      if (/^\/_.+$/.test(mountPoint)) {
        alert("Sorry, mount paths starting with an underscore are reserved for internal use.");
        return false;
      }
      if (!/^(\/[^\/\s]+)+$/.test(mountPoint)){
        alert("Sorry, you have to give a valid mount point, e.g.: /myPath");
        return false;
      }

      if (this.model.get("selectOptions")) {
        version = $("#foxx-version").val();
      }
      else {
        version = this.model.get("version");
      }

      var cfg = {};
      var opts = this.model.get("manifest").configuration;
      if (opts && _.keys(opts).length) {
        try {
          _.each(opts, function(opt, key) {
            var $el = $("#foxx_configs_" + key);
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
      }

      //find correct app install version
      var toCreate = this.model.collection.findWhere({
        name: self.model.get("name"),
        version: version
      });

      toCreate.collection.create({
        mount: mountPoint,
        name: toCreate.get("name"),
        version: toCreate.get("version"),
        options: {
          configuration: cfg
        }
      },
      {
        success: function() {
          window.modalView.hide();
          self.appsView.reload();
        },
        error: function(e, info) {
          if (info.responseText.indexOf("already used by") > -1) {
            alert("Mount-Path already in use.");
          } else if (info.responseText.indexOf("app is not defined") > -1) {
            //temp ignore this message, fix needs to be server-side
            window.modalView.hide();
            self.appsView.reload();
          } else {
            alert(info.statusText);
          }
        }
      });
    },

    selectSpecificVersion: function(version) {
      $(".modalSelect").val(version);
    },

    selectHighestVersion: function() {
      var versions = this.model.get("versions"),
      toRender = "0";

      _.each(versions, function(version) {
        if(version >= toRender) {
          toRender = version;
        }
      });

      $(".modalSelect").val(toRender);
    },

    render: function(){
      var activeVersion = this.model.get("activeVersion");

      if (activeVersion === 0 || activeVersion === undefined) {
        activeVersion = this.model.get("highestVersion");
      }

      $(this.el).html(this.template.render(this.model));

      return $(this.el);
    }

  });
}());

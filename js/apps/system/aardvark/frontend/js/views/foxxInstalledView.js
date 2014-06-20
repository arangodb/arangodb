/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true regexp: true*/
/*global Backbone, $, window, _, templateEngine, alert*/

(function() {
  "use strict";
  window.FoxxInstalledView = Backbone.View.extend({
    tagName: 'div',
    className: "tile",
    template: templateEngine.createTemplate("foxxInstalledView.ejs"),

    events: {
      //'click .install': 'installDialog',
      //'click .purge': 'removeDialog',
      'click .icon_arangodb_settings2': 'infoDialog'
    },

    renderVersion: function(e) {
      var name = this.model.get("name"),
      selectOptions = this.model.get("selectOptions"),
      versionToRender = $('#'+name+'Select').val();
      this.model.set("activeVersion", versionToRender);

      var toRender = this.model.collection.findWhere({
        name: name,
        version: versionToRender
      });

    },

    initialize: function(){
      _.bindAll(this, 'render');
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
          "Update", this.update.bind(this)
        ),
        window.modalView.createSuccessButton(
          "Install", this.installDialog.bind(this)
        )
      ];
      var buttonSystemInfoConfig = [
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
      this.showInfoMultipleVersionsMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Application Settings",
        buttonInfoMultipleVersionsConfig
      );
      this.showSystemInfoMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Application Settings",
        buttonSystemInfoConfig
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
      var versions, isSystem = false;
      if (this.model.get("isSystem")) {
        isSystem = true;
      } else {
        isSystem = false;
      }

      versions = this.model.get("versions");

      event.stopPropagation();
      if (isSystem === false && !versions) {
        this.showInfoMod(this.fillInfoValues());
      }
      else if (isSystem === false && versions) {
        this.showInfoMultipleVersionsMod(this.fillInfoValues());
      }
      else {
        this.showSystemInfoMod(this.fillInfoValues());
      }
    },

    installDialog: function(event) {
      window.modalView.hide();
      event.stopPropagation();
      this.showMod(this.fillValues());
    },

    update: function() {
      var url = this.model.get("git"),
      name = '',
      version = 'master',
      result;

      if (url === undefined || url === '') {
        // if no git is defined
        return;
      }

      result = this.collection.installFoxxFromGithub(url, name, version);
      if (result === true) {
        window.modalView.hide();
        window.App.applicationsView.reload();
      }
    },

    install: function() {
      var mountPoint = $("#mount-point").val(),
        version = "",
        regex = /^(\/[^\/\s]+)+$/,
        self = this;
      if (!regex.test(mountPoint)){
        alert("Sorry, you have to give a valid mount point, e.g.: /myPath");
        return false;
      }

      if (this.model.get("selectOptions")) {
        version = $("#foxx-version").val();
      }
      else {
        version = this.model.get("version");
      }

      //find correct app install version
      var toCreate = this.model.collection.findWhere({
        name: self.model.get("name"),
        version: version
      });

      //this.model.collection.create({
      toCreate.collection.create({
        mount: mountPoint,
        name: toCreate.get("name"),
        version: toCreate.get("version")
      },
      {
        success: function() {
          window.modalView.hide();
          self.appsView.reload();
        },
        error: function(e, info) {
          if (info.responseText.indexOf("already used by") > -1) {
            alert("Mount-Path already in use.");
          } else {
            alert(info.statusText);
          }
        }
      });
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

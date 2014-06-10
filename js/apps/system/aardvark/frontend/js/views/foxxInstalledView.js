/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true regexp: true*/
/*global Backbone, $, window, _, templateEngine, alert*/

(function() {
  "use strict";
  window.FoxxInstalledView = Backbone.View.extend({
    tagName: 'div',
    className: "tile",
    template: templateEngine.createTemplate("foxxInstalledView.ejs"),

    events: {
      'click .install': 'installDialog',
      'click .purge': 'removeDialog'
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
          "Remove", this.confirmRemoval.bind(this)
        )
      ];
      this.showMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Install Application",
        buttonConfig
      );
      this.showPurgeMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Remove Application",
        buttonPurgeConfig
      );
      this.appsView = this.options.appsView;
    },

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
      list.push(window.modalView.createReadOnlyEntry(
        "id_version", "Version", this.model.get("version")
      ));
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
      list.push(window.modalView.createReadOnlyEntry(
        "id_version", "Version", this.model.get("version")
      ));
      list.push(window.modalView.createReadOnlyEntry(
        "id_system", "System", isSystem
      ));
      return list;
    },

    removeDialog: function(event) {
      event.stopPropagation();
      var name = this.model.get("name");
      var mountinfo = this.model.collection.mountInfo(name);
      this.showPurgeMod(this.fillPurgeValues(mountinfo));
    },

    confirmRemoval: function(event) {
      var name = this.model.get("name");
      var result = this.model.collection.purgeFoxx(name);
      if (result === true) {
        window.modalView.hide();
        window.App.applicationsView.reload();
      }
    },

    installDialog: function(event) {
      event.stopPropagation();
      this.showMod(this.fillValues());
    },

    install: function() {
      var mountPoint = $("#mount-point").val(),
        regex = /^(\/[^\/\s]+)+$/,
        self = this;
      if (!regex.test(mountPoint)){
        alert("Sorry, you have to give a valid mount point, e.g.: /myPath");
        return false;
      } 
      this.model.collection.create({
        mount: mountPoint,
        name: this.model.get("name"),
        version: this.model.get("version")
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
      $(this.el).html(this.template.render(this.model));
      return $(this.el);
    }
  });
}());

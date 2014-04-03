/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true regexp: true*/
/*global Backbone, $, window, _, templateEngine, alert*/

(function() {
  "use strict";
  window.FoxxInstalledView = Backbone.View.extend({
    tagName: 'div',
    className: "tile",
    template: templateEngine.createTemplate("foxxInstalledView.ejs"),

    events: {
      'click .install': 'installDialog'
    },

    initialize: function(){
      _.bindAll(this, 'render');
      this.buttonConfig = [
        window.modalView.createSuccessButton(
          "Install", this.install.bind(this)
        )
      ];
      this.showMod = window.modalView.show.bind(
        window.modalView,
        "modalTable.ejs",
        "Install Application"
      );
      this.appsView = this.options.appsView;
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
        "Name", this.model.get("name")
      ));
      list.push(window.modalView.createTextEntry(
        "mount-point", "Mount", "/" + this.model.get("name"),
        "The path where the app can be reached."
      ));
      list.push(window.modalView.createReadOnlyEntry(
        "Version", this.model.get("version") 
      ));
      list.push(window.modalView.createReadOnlyEntry(
        "System", isSystem
      ));
      return list;
    },

    installDialog: function(event) {
      event.stopPropagation();
      this.showMod(this.buttonConfig, this.fillValues());
    },

    installFoxx: function(event) {
      event.stopPropagation();
      window.App.navigate(
        "application/available/" + encodeURIComponent(this.model.get("_key")),
        {
          trigger: true
        }
      );
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

/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, EJS, $, window, _, templateEngine, arangoHelper */

(function() {
  "use strict";
  window.foxxEditView = Backbone.View.extend({
    el: '#modalPlaceholder',
    initialize: function () {
    },

    template: templateEngine.createTemplate("foxxEditView.ejs"),

    render: function() {
      $(this.el).html(this.template.render(this.model));
      $('#change-foxx').modal('show');
      $('.modalTooltips').tooltip({
        placement: "left"
      });
      return this;
    },
    events: {
      "hidden #change-foxx"   : "hidden",
      // "click #deactivate"     : "deactivate",
      // "click #activate"       : "activate",
      "click #uninstall"      : "uninstall",
      "click #change"         : "changeFoxx"
    },
    
    checkMount: function(mount) {
      /*jslint regexp: true*/
      var regex = /^(\/[^\/\s]+)+$/;
      if (!regex.test(mount)){
        arangoHelper.arangoError("Please give a valid mount point, e.g.: /myPath");
        return false;
      }
      return true;
    },

    changeFoxx: function() {
      var mount = $("#change-mount-point").val();
      var failed = false;
      var app = this.model.get("app");
      var prefix = this.model.get("options").collectionPrefix;
      if (mount !== this.model.get("mount")) {
        if (this.checkMount(mount)) {
          $.ajax("foxx/move/" + this.model.get("_key"), {
            async: false,
            type: "PUT",
            data: JSON.stringify({
              mount: mount,
              app: app,
              prefix: prefix
            }),
            dataType: "json",
            error: function(data) {
              failed = true;
              arangoHelper.arangoError(data.responseText);
            }
          });
        } else {
          return;
        }
      }
      // TODO change version
      if (!failed) {
        this.hideModal();
      }
    },

    hidden: function () {
      this.undelegateEvents();
      window.App.navigate("applications", {trigger: true});
    },
    
    uninstall: function () {
      if (! this.model.isSystem) { 
        this.model.destroy({ wait: true });
        this.hideModal();
      }
    },
   
    /* not yet implemented
    deactivate: function () {
      this.model.save({active: false});
      this.hideModal();
    },
    */
    
    /* not yet implemented
    activate: function () {
      this.model.save({active: true});
      this.hideModal();
    },
    */

    hideModal: function () {
      $('#change-foxx').modal('hide');
    }
  });
}());

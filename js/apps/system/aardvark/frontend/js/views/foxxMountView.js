/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, forin: true, regexp: true */
/*global alert, Backbone, EJS, $, window, templateEngine */

window.foxxMountView = Backbone.View.extend({

  el: '#modalPlaceholder',

  template: templateEngine.createTemplate("foxxMountView.ejs"),

  events: {
    "hidden #install-foxx"  : "hidden",
    "click #cancel"         : "hideModal",
    "click .installFoxx"    : "install"
  },

  render: function(key) {
    var m = this.collection.findWhere({_key: key});
    this.m = m;
    $(this.el).html(this.template.render(m));
    $('#install-foxx').modal('show');
    $('.modalTooltips').tooltip({
      placement: "left"
    });
    return this;
  },

  hidden: function () {
    window.App.navigate("applications", {trigger: true});
  },
  
  install: function () {
    var mountPoint = $("#mount-point").val(),
      regex = /^(\/[^\/\s]+)+$/;
    if (!regex.test(mountPoint)){
      alert("Sorry, you have to give a valid mount point, e.g.: /myPath");
      return false;
    } 
    this.collection.create({
      mount: mountPoint,
      name: this.m.get("name"),
      version: this.m.get("version")
    });
    this.hideModal();
  },
  
  hideModal: function () {
    $('#install-foxx').modal('hide');
  }

});

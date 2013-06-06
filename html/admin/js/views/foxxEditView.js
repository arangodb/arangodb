/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global Backbone, EJS, $, window, _ */

window.foxxEditView = Backbone.View.extend({
  el: '#modalPlaceholder',
  initialize: function () {
  },

  template: new EJS({url: 'js/templates/foxxEditView.ejs'}),

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
    "click #uninstall"      : "uninstall",
    "click #deactivate"     : "deactivate",
    "click #activate"       : "activate"
  },
  hidden: function () {
    window.App.navigate("applications/installed", {trigger: true});
  },
  
  uninstall: function () {
    this.model.destroy();
    this.hideModal();
  },
  
  deactivate: function () {
    this.model.save({active: false});
    this.hideModal();
  },
  
  activate: function () {
    this.model.save({active: true});
    this.hideModal();
  },
  
  hideModal: function () {
    $('#change-foxx').modal('hide');
  }
});

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
    // "click #deactivate"     : "deactivate",
    // "click #activate"       : "activate",
    "click #uninstall"      : "uninstall"
  },
  hidden: function () {
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

window.foxxEditView = Backbone.View.extend({
  el: '#modalPlaceholder',
  initialize: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/foxxEditView.ejs'}),

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
    window.App.navigate("applications/installed");
  },
  
  uninstall: function () {
    this.model.destroy();
  },
  
  deactivate: function () {
    this.model.save({active: false});
  },
  
  activate: function () {
    this.model.save({active: true});
  }
});

window.foxxMountView = Backbone.View.extend({
  el: '#modalPlaceholder',
  m: {},
  
  initialize: function () {
    this.m = this.model.attributes;
    console.log(this.m);
  },
  template: new EJS({url: 'js/templates/foxxMountView.ejs'}),

  render: function() {
    $(this.el).html(this.template.render(this.model));
    
    $('#install-foxx').modal('show');
    $('.modalTooltips').tooltip({
      placement: "left"
    });
    return this;
  },
  events: {
    "hidden #install-foxx"   : "hidden",
    "click #cancel"         : "hideModal",
    "click #install"        : "install"
  },
  hidden: function () {
    window.App.navigate("applications/available", {trigger: true});
  },
  
  install: function () {
    var mountPoint = $("#mount-point").val(),
      regex = /^(\/[^\/\s]+)+$/;
    if (!regex.test(mountPoint)){
      alert("Sorry you have to give a valid path f.e.: /myPath");
    } else {
      var self = this;
      (new window.FoxxCollection()).create({
        mount: mountPoint,
        name: self.m.name,
        version: self.m.version
      });
      this.hideModal();
    }
  },
  
  hideModal: function () {
    $('#install-foxx').modal('hide');
  }
});

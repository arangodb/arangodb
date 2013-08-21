/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $*/

var footerView = Backbone.View.extend({
  el: '.footer',
  system: {},
  isOffline: false,

  initialize: function () {
    //also server online check
    this.updateVersion();
  },

  template: new EJS({url: 'js/templates/footerView.ejs'}),

  updateVersion: function () {
    var self = this;
    window.setInterval(function(){
      self.getVersion();
    }, 10000);
  },

  getVersion: function () {
    var self = this;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_admin/version",
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        if (self.isOffline === true) {
          self.isOffline = false;
          arangoHelper.removeNotifications();
          window.setTimeout(function(){
            arangoHelper.arangoNotification("Server connected");
          }, 1000);
        }

        self.system.name = data.server;
        self.system.version = data.version;
        self.renderVersion();
      },
      error: function (data) {
        self.isOffline = true;
        arangoHelper.arangoError("Server is offline");
      }
    });
  },

  renderVersion: function () {
    if (this.system.hasOwnProperty('name')) {
      $('.footer-right p').html(this.system.name + ' : ' + this.system.version);
    }
  },

  render: function () {
    $(this.el).html(this.template.text);
    this.getVersion();

    // only fill in version if we have a version number...
    return this;
  }

});

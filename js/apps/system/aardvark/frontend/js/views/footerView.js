/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global Backbone, EJS, $, arangoHelper, window*/

var footerView = Backbone.View.extend({
  el: '.footer',
  system: {},
  isOffline: false,

  initialize: function () {
    //also server online check
    var self = this;
    window.setInterval(function(){
      self.getVersion();
    }, 15000);
    self.getVersion();
  },

  template: new EJS({url: 'js/templates/footerView.ejs'}),

  getVersion: function () {
    var self = this;

    // always retry this call, because it also checks if the server is online
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/version",
      contentType: "application/json",
      processData: false,
      async: true,
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
    
    if (! self.system.hasOwnProperty('database')) {
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/database/current",
        contentType: "application/json",
        processData: false,
        async: true,
        success: function(data) {
          var name = data.result.name;
          self.system.database = name;
          $('#databaseName').html(name);
          if (name === '_system') {
            // show "logs" button
            $('.logs-menu').css('visibility', 'visible');
          }
          else {
            // hide "logs" button
            $('.logs-menu').css('visibility', 'hidden');
            $('.logs-menu').css('display', 'none');
          }
          self.renderVersion();
        }
      });
    }
  },

  renderVersion: function () {
    if (this.system.hasOwnProperty('database') && this.system.hasOwnProperty('name')) {
      var tag = 'Server: ' + this.system.name + ' ' + this.system.version + 
                ', Database: ' + this.system.database;
      $('.footer-right p').html(tag);
    }
  },

  render: function () {
    $(this.el).html(this.template.text);
    this.getVersion();

    // only fill in version if we have a version number...
    return this;
  }

});

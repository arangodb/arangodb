/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global Backbone, EJS, $, arangoHelper, window*/

var footerView = Backbone.View.extend({
  el: '.footer',
  system: {},
  isOffline: true,
  dbSelectionView: null,

  initialize: function () {
    //also server online check
    var self = this;
    window.setInterval(function(){
      self.getVersion();
    }, 15000);
    this.dbSelectionView = new window.DBSelectionView({
      collection: arangoDatabase
    });
    
    self.getVersion();
  },

  template: templateEngine.createTemplate("footerView.ejs"),

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
          self.system.name = data.server;
          self.system.version = data.version;
          self.render();
        }
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
            // show dbs menues
            $('#databaseNavi').css('display','inline');
            $('#databaseNaviSelect').css('display','inline');
          }
          else {
            // hide "logs" button
            $('.logs-menu').css('visibility', 'hidden');
            $('.logs-menu').css('display', 'none');
          }
          self.render();
        }
      });
    }
  },

  renderVersion: function () {
    if (this.system.hasOwnProperty('database') && this.system.hasOwnProperty('name')) {
      $(this.el).html(this.template.render({
        name: this.system.name,
        version: this.system.version,
        database: this.system.database,
        margin: this.resizeMargin
      }));
      this.dbSelectionView.render($("#selectDB"));
     /* 
          var tag = 'Server: ' + this.system.name + ' ' + this.system.version + 
        ', Database: ' + this.system.database;
      $('.footer-right p').html(tag);
      */
    }
  },

  handleResize: function(newMargin) {
    this.resizeMargin = newMargin;
    this.render();
  },

  render: function () {
    if (!this.system.version) {
      this.getVersion();
    }
    $(this.el).html(this.template.render({
      name: this.system.name,
      version: this.system.version,
      database: this.system.database,
      margin: this.resizeMargin
    }));
    this.dbSelectionView.render($("#selectDB"));
    return this;
  }

});

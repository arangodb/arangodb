/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $*/

var footerView = Backbone.View.extend({
  el: '.footer',
  initialize: function () {
    var self = this;
    self.system = { };
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_admin/version",
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        self.system.name = data.server;
        self.system.version = data.version;
      }
    });
  },

  template: new EJS({url: 'js/templates/footerView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);

    // only fill in version if we have a version number...
    if (this.system.hasOwnProperty('name')) {
      $('.footer-right p').html(this.system.name + ' : ' + this.system.version);
    }
    return this;
  }

});

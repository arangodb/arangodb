/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, arangoHelper, window, setTimeout, $*/

var loginView = Backbone.View.extend({
  el: '#content',
  el2: '.header',
  el3: '.footer',

  init: function () {
  },

  events: {
    "click #submitLogin" : "login",
    "keydown #loginUsername" : "checkKey",
    "keydown #loginPassword" : "checkKey"
  },

  template: new EJS({url: 'js/templates/loginView.ejs'}),

  render: function() {
    this.addDummyUser();

    $(this.el).html(this.template.text);
    $(this.el2).hide();
    $(this.el3).hide();
    $.gritter.removeAll();

    $('#loginUsername').focus();

    //DEVELOPMENT 
    $('#loginUsername').val('admin');
    $('#loginPassword').val('admin');

    return this;
  },

  addDummyUser: function () {
    this.collection.add({
      "userName" : "admin",
      "sessionId" : "abc123",
      "password" :"admin",
      "userId" : 1
    });
  },

  checkKey: function (e) {
    if (e.keyCode === 13) {
      this.login();
    }
  },

  login: function () {
    var username = $('#loginUsername').val();
    var password = $('#loginPassword').val();

    if (username === '' || password === '') {
      arangoHelper.arangoNotification("Please fill out required fields");
      return;
    }
    var callback = this.collection.login(username, password);

    if (callback === true) {
      $(this.el2).show();
      $(this.el3).show();
      window.App.navigate("/", {trigger: true});
      $('#currentUser').text(username);
      this.collection.loadUserSettings();
    }

  }

});

/* jshint strict: false */
/* global Backbone, $, window, arangoHelper */
window.Users = Backbone.Model.extend({
  defaults: {
    user: '',
    active: false,
    extra: {}
  },

  idAttribute: 'user',

  parse: function (d) {
    this.isNotNew = true;
    return d;
  },

  isNew: function () {
    return !this.isNotNew;
  },

  url: function () {
    if (this.isNew()) {
      return arangoHelper.databaseUrl('/_api/user');
    }
    if (this.get('user') !== '') {
      return arangoHelper.databaseUrl('/_api/user/' + this.get('user'));
    }
    return arangoHelper.databaseUrl('/_api/user');
  },

  checkPassword: function (passwd, callback) {
    $.ajax({
      cache: false,
      type: 'POST',
      url: arangoHelper.databaseUrl('/_api/user/' + this.get('user')),
      data: JSON.stringify({ passwd: passwd }),
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        callback(false, data);
      },
      error: function (data) {
        callback(true, data);
      }
    });
  },

  setPassword: function (passwd) {
    $.ajax({
      cache: false,
      type: 'PATCH',
      url: arangoHelper.databaseUrl('/_api/user/' + this.get('user')),
      data: JSON.stringify({ passwd: passwd }),
      contentType: 'application/json',
      processData: false
    });
  },

  setExtras: function (name, img, callback) {
    $.ajax({
      cache: false,
      type: 'PATCH',
      url: arangoHelper.databaseUrl('/_api/user/' + this.get('user')),
      data: JSON.stringify({'extra': {'name': name, 'img': img}}),
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        callback(false, data);
      },
      error: function () {
        callback(true);
      }
    });
  }

});
